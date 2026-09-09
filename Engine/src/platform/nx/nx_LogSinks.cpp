#include "nx_LogSinks.hpp"

#include <switch.h>

#include <arpa/inet.h>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>

namespace doggo::platform::nx
{
  namespace
  {
    constexpr int          NxlinkConnectAttempts       = 4;
    constexpr std::int64_t NxlinkRetryDelayNanoseconds = 50'000'000;

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

  NxlinkLogSink::~NxlinkLogSink()
  {
    finalize();
  }

  bool NxlinkLogSink::initialize() noexcept
  {
    if ( isConnected() )
    {
      return true;
    }

    // hbloader fills this address only when the NRO was launched by nxlink.
    if ( __nxlink_host.s_addr == 0 )
    {
      return false;
    }

    if ( R_FAILED( socketInitializeDefault() ) )
    {
      return false;
    }
    mIsSocketInitialized = true;

    // Keep stdout bound to the on-device console. The logger explicitly fans
    // each formatted record out to this socket and ConsoleLogSink instead.
    for ( int attempt = 0; attempt < NxlinkConnectAttempts; ++attempt )
    {
      mSocket = nxlinkConnectToHost( false, false );
      if ( mSocket >= 0 )
      {
        return true;
      }

      if ( attempt + 1 < NxlinkConnectAttempts )
      {
        svcSleepThread( NxlinkRetryDelayNanoseconds );
      }
    }

    finalize();
    return false;
  }

  void NxlinkLogSink::finalize() noexcept
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

  bool NxlinkLogSink::isConnected() const noexcept
  {
    return mSocket >= 0;
  }

  void NxlinkLogSink::write( const std::string_view text ) noexcept
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
