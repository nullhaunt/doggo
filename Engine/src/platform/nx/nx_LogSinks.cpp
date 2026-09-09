#include "doggo/platform/nx/nx_LogSinks.hpp"

#include "doggo/dev/dev_RemoteLogProtocol.hpp"

#include <switch.h>

#include <arpa/inet.h>
#include <cstdio>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace doggo::platform::nx
{
  namespace
  {
#ifdef MSG_NOSIGNAL
    constexpr int SendFlags = MSG_NOSIGNAL;
#else
    constexpr int SendFlags = 0;
#endif
  }  // namespace

  void ConsoleLogSink::write( const std::string_view text ) noexcept
  {
    std::fwrite( text.data(), 1, text.size(), stdout );
  }

  void ConsoleLogSink::flush() noexcept
  {
    std::fflush( stdout );
  }

  DoggoDevLogSink::~DoggoDevLogSink()
  {
    finalize();
  }

  bool DoggoDevLogSink::initialize() noexcept
  {
    if ( isConnected() )
    {
      return true;
    }

    // hbloader fills this address only when the NRO was launched by netloader.
    if ( __nxlink_host.s_addr == 0 )
    {
      return false;
    }

    if ( R_FAILED( socketInitializeDefault() ) )
    {
      return false;
    }
    mIsSocketInitialized = true;

    const int listener = socket( AF_INET, SOCK_STREAM, 0 );
    if ( listener < 0 )
    {
      finalize();
      return false;
    }

    constexpr int isReusableAddress = 1;
    if ( setsockopt( listener, SOL_SOCKET, SO_REUSEADDR, &isReusableAddress, sizeof( isReusableAddress ) ) != 0 )
    {
      close( listener );
      finalize();
      return false;
    }

    sockaddr_in address     = {};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl( INADDR_ANY );
    address.sin_port        = htons( dev::RemoteLogPort );
    if ( bind( listener, reinterpret_cast<const sockaddr *>( &address ), sizeof( address ) ) != 0 ||
         listen( listener, 1 ) != 0 )
    {
      close( listener );
      finalize();
      return false;
    }

    // WSL can always initiate this connection to the Switch, while the
    // traditional nxlink callback into a WSL NAT guest requires forwarding.
    pollfd descriptor = {};
    descriptor.fd     = listener;
    descriptor.events = POLLIN;
    if ( poll( &descriptor, 1, dev::RemoteLogAttachTimeoutMilliseconds ) <= 0 || ( descriptor.revents & POLLIN ) == 0 )
    {
      close( listener );
      finalize();
      return false;
    }

    const int connection = accept( listener, nullptr, nullptr );
    close( listener );
    mSocket = connection;
    if ( !isConnected() )
    {
      finalize();
      return false;
    }

    write( std::string_view{ dev::RemoteLogPreamble.data(), dev::RemoteLogPreamble.size() } );
    return isConnected();
  }

  void DoggoDevLogSink::finalize() noexcept
  {
    if ( mSocket >= 0 )
    {
      close( mSocket );
      mSocket = -1;
    }

    if ( mIsSocketInitialized )
    {
      socketExit();
      mIsSocketInitialized = false;
    }
  }

  bool DoggoDevLogSink::isConnected() const noexcept
  {
    return mSocket >= 0;
  }

  void DoggoDevLogSink::write( const std::string_view text ) noexcept
  {
    std::size_t sentSize = 0;
    while ( isConnected() && sentSize < text.size() )
    {
      const ssize_t result = send( mSocket, text.data() + sentSize, text.size() - sentSize, SendFlags );
      if ( result <= 0 )
      {
        finalize();
        return;
      }

      sentSize += static_cast<std::size_t>( result );
    }
  }
}  // namespace doggo::platform::nx
