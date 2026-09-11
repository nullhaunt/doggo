#pragma once

#include "doggo/doggo_Macro.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace doggo::platform::nx
{
  enum class RomFsReadStatus : std::uint8_t
  {
    Success,
    NotInitialized,
    InvalidArgument,
    OpenFailed,
    StatFailed,
    DestinationTooSmall,
    ReadFailed,
    CloseFailed,
  };

  struct RomFsReadReport final
  {
      RomFsReadStatus status       = RomFsReadStatus::NotInitialized;
      int             error_number = 0;
      std::size_t     file_size    = 0;
      std::size_t     bytes_read   = 0;
  };

  // Read-only view of the application ROMFS. romfsMountSelf selects the
  // embedded NRO image today and the current process image for a future NSO.
  class RomFs final
  {
      DOGGO_DISALLOW_COPY( RomFs );
      DOGGO_DISALLOW_MOVE( RomFs );

    public:
      RomFs() noexcept = default;
      ~RomFs();

      // Returns a libnx Result value. Zero indicates success.
      [[nodiscard]] std::uint32_t initialize() noexcept;
      [[nodiscard]] std::uint32_t finalize() noexcept;

      [[nodiscard]] RomFsReadReport readFile( const char * path, std::span<std::uint8_t> destination ) const noexcept;

      [[nodiscard]] bool isInitialized() const noexcept;

    private:
      bool mIsInitialized = false;
  };
}  // namespace doggo::platform::nx
