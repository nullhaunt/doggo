#include "Network.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#ifdef _WIN32
  #include <ws2tcpip.h>
#else
  #include <netdb.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif

namespace
{
#ifdef _WIN32
  using IoResult = int;
  using IoSize   = int;
#else
  using IoResult = ssize_t;
  using IoSize   = std::size_t;
#endif

  [[nodiscard]] int socketErrorCode() noexcept
  {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
  }

  [[nodiscard]] bool isInterrupted( const int error ) noexcept
  {
#ifdef _WIN32
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
  }

  [[nodiscard]] std::string socketErrorText( const int error )
  {
#ifdef _WIN32
    return "Winsock error " + std::to_string( error );
#else
    return std::strerror( error );
#endif
  }

  [[nodiscard]] int sendFlags() noexcept
  {
#ifdef MSG_NOSIGNAL
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
  }

  [[nodiscard]] IoSize ioSize( const std::size_t size ) noexcept
  {
#ifdef _WIN32
    return static_cast<int>( std::min<std::size_t>( size, std::numeric_limits<int>::max() ) );
#else
    return size;
#endif
  }
}  // namespace

namespace doggo::devtool
{
  NetworkRuntime::NetworkRuntime()
  {
#ifdef _WIN32
    WSADATA   data   = {};
    const int result = WSAStartup( MAKEWORD( 2, 2 ), &data );
    if ( result != 0 )
    {
      throw std::runtime_error{ "Unable to initialize Winsock: " + socketErrorText( result ) };
    }
#endif
  }

  NetworkRuntime::~NetworkRuntime()
  {
#ifdef _WIN32
    WSACleanup();
#endif
  }

  Socket::Socket( const NativeSocket handle ) noexcept
      : mHandle{ handle }
  {
  }

  Socket::~Socket()
  {
    close();
  }

  Socket::Socket( Socket && other ) noexcept
      : mHandle{ other.mHandle }
  {
    other.mHandle = InvalidSocket;
  }

  Socket & Socket::operator=( Socket && other ) noexcept
  {
    if ( this != &other )
    {
      close();
      mHandle       = other.mHandle;
      other.mHandle = InvalidSocket;
    }
    return *this;
  }

  bool Socket::isOpen() const noexcept
  {
    return mHandle != InvalidSocket;
  }

  NativeSocket Socket::nativeHandle() const noexcept
  {
    return mHandle;
  }

  void Socket::close() noexcept
  {
    if ( !isOpen() )
    {
      return;
    }

#ifdef _WIN32
    closesocket( mHandle );
#else
    ::close( mHandle );
#endif
    mHandle = InvalidSocket;
  }

  Socket connectTcp( const std::string_view host, const std::uint16_t port )
  {
    const std::string hostText{ host };
    const std::string service = std::to_string( port );

    addrinfo hints    = {};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo * addresses     = nullptr;
    const int  resolveResult = getaddrinfo( hostText.c_str(), service.c_str(), &hints, &addresses );
    if ( resolveResult != 0 )
    {
#ifdef _WIN32
      const char * const reason = gai_strerrorA( resolveResult );
#else
      const char * const reason = gai_strerror( resolveResult );
#endif
      throw std::runtime_error{ "Unable to resolve " + hostText + ": " + ( reason ? reason : "unknown error" ) };
    }

    int lastError = 0;
    for ( const addrinfo * address = addresses; address; address = address->ai_next )
    {
      Socket socket{ ::socket( address->ai_family, address->ai_socktype, address->ai_protocol ) };
      if ( !socket.isOpen() )
      {
        lastError = socketErrorCode();
        continue;
      }

#ifdef SO_NOSIGPIPE
      const int shouldSuppressSigpipe = 1;
      if ( setsockopt( socket.nativeHandle(),
                       SOL_SOCKET,
                       SO_NOSIGPIPE,
                       &shouldSuppressSigpipe,
                       static_cast<socklen_t>( sizeof( shouldSuppressSigpipe ) ) ) != 0 )
      {
        lastError = socketErrorCode();
        continue;
      }
#endif

#ifdef _WIN32
      const int addressSize = static_cast<int>( address->ai_addrlen );
#else
      const socklen_t addressSize = static_cast<socklen_t>( address->ai_addrlen );
#endif
      if ( ::connect( socket.nativeHandle(), address->ai_addr, addressSize ) == 0 )
      {
        freeaddrinfo( addresses );
        return socket;
      }

      lastError = socketErrorCode();
    }

    freeaddrinfo( addresses );
    throw std::runtime_error{
        "Unable to connect to " + hostText + ':' + service + ": " + socketErrorText( lastError ) };
  }

  void sendAll( const Socket & socket, const void * const data, const std::size_t size )
  {
    const auto * bytes  = static_cast<const char *>( data );
    std::size_t  offset = 0;
    while ( offset < size )
    {
      const IoResult result = ::send( socket.nativeHandle(), bytes + offset, ioSize( size - offset ), sendFlags() );
      if ( result > 0 )
      {
        offset += static_cast<std::size_t>( result );
        continue;
      }

      const int error = socketErrorCode();
      if ( result < 0 && isInterrupted( error ) )
      {
        continue;
      }

      throw std::runtime_error{ "Socket send failed: " + socketErrorText( error ) };
    }
  }

  void receiveAll( const Socket & socket, void * const data, const std::size_t size )
  {
    auto *      bytes  = static_cast<char *>( data );
    std::size_t offset = 0;
    while ( offset < size )
    {
      const std::size_t received = receiveSome( socket, bytes + offset, size - offset );
      if ( received == 0 )
      {
        throw std::runtime_error{ "The peer closed the connection before its response was complete." };
      }
      offset += received;
    }
  }

  std::size_t receiveSome( const Socket & socket, void * const data, const std::size_t capacity )
  {
    while ( true )
    {
      const IoResult result = ::recv( socket.nativeHandle(), static_cast<char *>( data ), ioSize( capacity ), 0 );
      if ( result >= 0 )
      {
        return static_cast<std::size_t>( result );
      }

      const int error = socketErrorCode();
      if ( isInterrupted( error ) )
      {
        continue;
      }

      throw std::runtime_error{ "Socket receive failed: " + socketErrorText( error ) };
    }
  }
}  // namespace doggo::devtool
