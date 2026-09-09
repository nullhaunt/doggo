#pragma once

#include <doggo/doggo_Macro.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
#endif

namespace doggo::devtool
{
#ifdef _WIN32
  using NativeSocket                          = SOCKET;
  inline constexpr NativeSocket InvalidSocket = INVALID_SOCKET;
#else
  using NativeSocket                          = int;
  inline constexpr NativeSocket InvalidSocket = -1;
#endif

  class NetworkRuntime final
  {
      DOGGO_DISALLOW_COPY( NetworkRuntime );
      DOGGO_DISALLOW_MOVE( NetworkRuntime );

    public:
      NetworkRuntime();
      ~NetworkRuntime();
  };

  class Socket final
  {
      DOGGO_DISALLOW_COPY( Socket );

    public:
      Socket() noexcept = default;
      explicit Socket( NativeSocket handle ) noexcept;
      ~Socket();

      Socket( Socket && other ) noexcept;
      Socket & operator=( Socket && other ) noexcept;

      [[nodiscard]] bool         isOpen() const noexcept;
      [[nodiscard]] NativeSocket nativeHandle() const noexcept;
      void                       close() noexcept;

    private:
      NativeSocket mHandle = InvalidSocket;
  };

  [[nodiscard]] Socket connectTcp( std::string_view host, std::uint16_t port );

  void sendAll( const Socket & socket, const void * data, std::size_t size );
  void receiveAll( const Socket & socket, void * data, std::size_t size );

  [[nodiscard]] std::size_t receiveSome( const Socket & socket, void * data, std::size_t capacity );
}  // namespace doggo::devtool
