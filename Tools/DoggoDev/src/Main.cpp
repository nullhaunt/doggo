#include "Arguments.hpp"
#include "NetloaderClient.hpp"
#include "Network.hpp"

#include <doggo/dev/dev_RemoteLogProtocol.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
  [[nodiscard]] doggo::devtool::Socket
  connectLogStream( const std::string & host, const std::uint16_t port, const std::chrono::milliseconds timeout )
  {
    const auto  deadline = std::chrono::steady_clock::now() + timeout;
    std::string lastError;

    do
    {
      try
      {
        return doggo::devtool::connectTcp( host, port );
      }
      catch ( const std::runtime_error & error )
      {
        lastError = error.what();
      }

      std::this_thread::sleep_for( std::chrono::milliseconds{ 100 } );
    } while ( std::chrono::steady_clock::now() < deadline );

    throw std::runtime_error{ "DOGGO did not open its log stream within " +
                              std::to_string( timeout.count() ) +
                              " ms. Last connection error: " +
                              lastError };
  }

  void validateLogPreamble( const doggo::devtool::Socket & socket )
  {
    std::array<char, doggo::dev::RemoteLogPreamble.size()> preamble = {};
    doggo::devtool::receiveAll( socket, preamble.data(), preamble.size() );
    if ( !std::equal( preamble.begin(), preamble.end(), doggo::dev::RemoteLogPreamble.begin() ) )
    {
      throw std::runtime_error{ "The service on the log port is not a compatible DOGGO log stream." };
    }
  }

  void streamLogs( const doggo::devtool::Socket & socket )
  {
    std::array<char, 4096> buffer = {};
    while ( true )
    {
      const std::size_t received = doggo::devtool::receiveSome( socket, buffer.data(), buffer.size() );
      if ( received == 0 )
      {
        break;
      }

      std::cout.write( buffer.data(), static_cast<std::streamsize>( received ) );
      std::cout.flush();
    }
  }
}  // namespace

int main( const int argc, char * const argv[] )
{
  const std::optional<doggo::devtool::Options> parsedOptions = doggo::devtool::parseOptions( argc, argv );
  if ( !parsedOptions )
  {
    return EXIT_FAILURE;
  }
  const doggo::devtool::Options & options = *parsedOptions;

  try
  {
    const std::filesystem::path nro = std::filesystem::absolute( options.nro );
    if ( !std::filesystem::is_regular_file( nro ) )
    {
      throw std::runtime_error{ "NRO not found: " + nro.string() };
    }

    [[maybe_unused]] doggo::devtool::NetworkRuntime network;
    const doggo::devtool::NetloaderClient           netloader{ options.switch_address, doggo::dev::NetloaderPort };

    std::cerr << "Deploying " << nro << " to " << options.switch_address << ':' << doggo::dev::NetloaderPort << "...\n";
    const doggo::devtool::UploadResult uploaded =
        netloader.upload( nro, options.remote_path, options.application_arguments );
    std::cerr << "Uploaded " << uploaded.source_bytes << " bytes (" << uploaded.compressed_bytes
              << " compressed bytes).\n"
              << "Waiting for DOGGO at " << options.switch_address << ':' << doggo::dev::RemoteLogPort << "...\n";

    doggo::devtool::Socket logSocket =
        connectLogStream( options.switch_address,
                          doggo::dev::RemoteLogPort,
                          std::chrono::milliseconds{ doggo::dev::RemoteLogAttachTimeoutMilliseconds } );
    validateLogPreamble( logSocket );
    std::cerr << "Connected. Streaming logs; press Ctrl+C to detach.\n";

    streamLogs( logSocket );
    std::cerr << "DOGGO log stream closed.\n";
    return EXIT_SUCCESS;
  }
  catch ( const std::exception & error )
  {
    std::cerr << "doggo-dev: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
