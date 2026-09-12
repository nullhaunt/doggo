#include "NetloaderClient.hpp"

#include "Network.hpp"

#include <zlib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
  constexpr std::size_t TransferChunkSize = 16 * 1024;

  // hbmenu stores argc, argv[0], and forwarded arguments in one 0x400-byte
  // buffer. Netloaded NROs use "sdmc:/switch/<remote path>" as argv[0].
  constexpr std::size_t      ArgumentBufferCapacity = 0x400;
  constexpr std::size_t      ArgumentHeaderSize     = sizeof( std::uint32_t );
  constexpr std::size_t      ArgumentGuardSize      = 1;
  constexpr std::string_view HbmenuUploadRoot{ "sdmc:/switch/" };
  constexpr std::size_t RemotePathCapacity = ArgumentBufferCapacity - ArgumentHeaderSize - HbmenuUploadRoot.size() - 2;

  void sendUint32( const doggo::devtool::Socket & socket, const std::uint32_t value )
  {
    const std::array<std::uint8_t, 4> bytes{
        static_cast<std::uint8_t>( value ),
        static_cast<std::uint8_t>( value >> 8u ),
        static_cast<std::uint8_t>( value >> 16u ),
        static_cast<std::uint8_t>( value >> 24u ),
    };
    doggo::devtool::sendAll( socket, bytes.data(), bytes.size() );
  }

  [[nodiscard]] std::int32_t receiveInt32( const doggo::devtool::Socket & socket )
  {
    std::array<std::uint8_t, 4> bytes = {};
    doggo::devtool::receiveAll( socket, bytes.data(), bytes.size() );

    const std::uint32_t value = static_cast<std::uint32_t>( bytes[ 0 ] ) |
                                static_cast<std::uint32_t>( bytes[ 1 ] ) << 8u |
                                static_cast<std::uint32_t>( bytes[ 2 ] ) << 16u |
                                static_cast<std::uint32_t>( bytes[ 3 ] ) << 24u;
    return static_cast<std::int32_t>( value );
  }

  void validateResponse( const std::int32_t response, const std::string_view stage )
  {
    if ( response == 0 )
    {
      return;
    }

    std::string reason;
    switch ( response )
    {
      case -1:
        reason = "hbmenu could not create or finish the uploaded file";
        break;

      case -2:
        reason = "the Switch does not have enough free space";
        break;

      case -3:
        reason = "hbmenu does not have enough memory";
        break;

      default:
        reason = "hbmenu returned error " + std::to_string( response );
        break;
    }

    throw std::runtime_error{ std::string{ stage } + " failed: " + reason + '.' };
  }

  [[nodiscard]] std::vector<char> makeCommandPayload( const std::vector<std::string> & arguments,
                                                      const std::size_t                capacity )
  {
    std::vector<char> payload;
    for ( const std::string & argument : arguments )
    {
      if ( argument.find( '\0' ) != std::string::npos )
      {
        throw std::runtime_error{ "Application arguments cannot contain NUL bytes." };
      }
      if ( payload.size() + argument.size() + 1 > capacity )
      {
        throw std::runtime_error{ "Application arguments exceed hbmenu's remaining " +
                                  std::to_string( capacity ) +
                                  "-byte buffer for this remote path." };
      }

      payload.insert( payload.end(), argument.begin(), argument.end() );
      payload.push_back( '\0' );
    }
    return payload;
  }

  class DeflateState final
  {
    public:
      DeflateState( const DeflateState & )             = delete;
      DeflateState( DeflateState && )                  = delete;
      DeflateState & operator=( const DeflateState & ) = delete;
      DeflateState & operator=( DeflateState && )      = delete;

      DeflateState()
      {
        mStream.zalloc = Z_NULL;
        mStream.zfree  = Z_NULL;
        mStream.opaque = Z_NULL;

        const int result = deflateInit( &mStream, Z_DEFAULT_COMPRESSION );
        if ( result != Z_OK )
        {
          throw std::runtime_error{
              "Unable to initialize NRO compression (zlib error " + std::to_string( result ) + ")." };
        }
      }

      ~DeflateState()
      {
        deflateEnd( &mStream );
      }

      [[nodiscard]] z_stream & stream() noexcept
      {
        return mStream;
      }

    private:
      z_stream mStream = {};
  };

  [[nodiscard]] doggo::devtool::UploadResult
  sendCompressedFile( const doggo::devtool::Socket & socket, std::ifstream & input, const std::uint64_t expectedSize )
  {
    std::array<unsigned char, TransferChunkSize> inputBuffer  = {};
    std::array<unsigned char, TransferChunkSize> outputBuffer = {};
    DeflateState                                 state;
    z_stream &                                   stream = state.stream();
    doggo::devtool::UploadResult                 result;

    int compressionResult = Z_OK;
    int flush             = Z_NO_FLUSH;
    do
    {
      input.read( reinterpret_cast<char *>( inputBuffer.data() ), inputBuffer.size() );
      const std::streamsize readSize = input.gcount();
      if ( input.bad() || ( input.fail() && !input.eof() ) )
      {
        throw std::runtime_error{ "Unable to read the NRO." };
      }

      result.source_bytes += static_cast<std::uint64_t>( readSize );
      stream.next_in  = inputBuffer.data();
      stream.avail_in = static_cast<uInt>( readSize );
      flush           = input.eof() ? Z_FINISH : Z_NO_FLUSH;

      do
      {
        stream.next_out   = outputBuffer.data();
        stream.avail_out  = static_cast<uInt>( outputBuffer.size() );
        compressionResult = deflate( &stream, flush );
        if ( compressionResult == Z_STREAM_ERROR )
        {
          throw std::runtime_error{ "NRO compression failed." };
        }

        const std::size_t outputSize = outputBuffer.size() - stream.avail_out;
        if ( outputSize != 0 )
        {
          sendUint32( socket, static_cast<std::uint32_t>( outputSize ) );
          doggo::devtool::sendAll( socket, outputBuffer.data(), outputSize );
          result.compressed_bytes += outputSize;
        }
      } while ( stream.avail_out == 0 );

      if ( stream.avail_in != 0 )
      {
        throw std::runtime_error{ "NRO compression did not consume its input." };
      }
    } while ( flush != Z_FINISH );

    if ( compressionResult != Z_STREAM_END || result.source_bytes != expectedSize )
    {
      throw std::runtime_error{ "NRO compression ended before the complete file was read." };
    }

    return result;
  }
}  // namespace

namespace doggo::devtool
{
  NetloaderClient::NetloaderClient( std::string host, const std::uint16_t port )
      : mHost{ std::move( host ) }
      , mPort{ port }
  {
  }

  UploadResult NetloaderClient::upload( const std::filesystem::path &    nro,
                                        const std::string_view           remotePath,
                                        const std::vector<std::string> & applicationArguments ) const
  {
    if ( !std::filesystem::is_regular_file( nro ) )
    {
      throw std::runtime_error{ "NRO not found: " + nro.string() };
    }

    const std::uintmax_t fileSize = std::filesystem::file_size( nro );
    if ( fileSize == 0 )
    {
      throw std::runtime_error{ "The NRO is empty: " + nro.string() };
    }

    if ( fileSize > static_cast<std::uintmax_t>( std::numeric_limits<std::int32_t>::max() ) )
    {
      throw std::runtime_error{ "The NRO is too large for hbmenu's netloader protocol." };
    }

    if ( remotePath.empty() || remotePath.size() > RemotePathCapacity )
    {
      throw std::runtime_error{
          "The remote path must contain between 1 and " + std::to_string( RemotePathCapacity ) + " bytes." };
    }

    if ( remotePath.find( '\0' ) != std::string_view::npos )
    {
      throw std::runtime_error{ "The remote path cannot contain NUL bytes." };
    }

    std::ifstream input{ nro, std::ios::binary };
    if ( !input )
    {
      throw std::runtime_error{ "Unable to open NRO: " + nro.string() };
    }

    const std::size_t commandCapacity = ArgumentBufferCapacity -
                                        ArgumentHeaderSize -
                                        HbmenuUploadRoot.size() -
                                        remotePath.size() -
                                        1 -
                                        ArgumentGuardSize;
    const std::vector<char> commandPayload = makeCommandPayload( applicationArguments, commandCapacity );
    Socket                  socket         = connectTcp( mHost, mPort );

    sendUint32( socket, static_cast<std::uint32_t>( remotePath.size() ) );
    sendAll( socket, remotePath.data(), remotePath.size() );
    sendUint32( socket, static_cast<std::uint32_t>( fileSize ) );
    validateResponse( receiveInt32( socket ), "Upload preparation" );

    UploadResult result = sendCompressedFile( socket, input, fileSize );
    validateResponse( receiveInt32( socket ), "Upload" );

    sendUint32( socket, static_cast<std::uint32_t>( commandPayload.size() ) );
    if ( !commandPayload.empty() )
    {
      sendAll( socket, commandPayload.data(), commandPayload.size() );
    }

    return result;
  }
}  // namespace doggo::devtool
