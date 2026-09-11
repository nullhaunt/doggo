#pragma once

#include "doggo/doggo_Macro.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace doggo::platform::nx
{
  enum class ApplicationStorageStatus : std::uint8_t
  {
    Success,
    NotInitialized,
    InvalidName,
    NotFound,
    DirectoryFailed,
    StatFailed,
    DestinationTooSmall,
    OpenFailed,
    ReadFailed,
    WriteFailed,
    FlushFailed,
    CloseFailed,
    RemoveFailed,
    RenameFailed,
    CommitFailed,
  };

  struct ApplicationStorageReport final
  {
      ApplicationStorageStatus status            = ApplicationStorageStatus::NotInitialized;
      std::uint32_t            native_result     = 0;
      int                      error_number      = 0;
      std::size_t              file_size         = 0;
      std::size_t              bytes_transferred = 0;
  };

  // Backend-neutral boundary for writable engine data. Gate 0 uses SDMC for
  // both launch modes; native application savedata for installed builds is
  // intentionally deferred behind this class.
  class ApplicationStorage final
  {
      DOGGO_DISALLOW_COPY( ApplicationStorage );
      DOGGO_DISALLOW_MOVE( ApplicationStorage );

    public:
      ApplicationStorage() noexcept = default;
      ~ApplicationStorage();

      // Returns a libnx Result value. Zero indicates success.
      [[nodiscard]] std::uint32_t initialize() noexcept;
      [[nodiscard]] std::uint32_t finalize() noexcept;

      [[nodiscard]] ApplicationStorageReport readCommittedFile( std::string_view        name,
                                                                std::span<std::uint8_t> destination ) noexcept;

      [[nodiscard]] ApplicationStorageReport writeCommittedFile( std::string_view              name,
                                                                 std::span<const std::uint8_t> source ) noexcept;

      [[nodiscard]] std::string_view backendName() const noexcept;
      [[nodiscard]] std::string_view rootPath() const noexcept;
      [[nodiscard]] bool             isInitialized() const noexcept;

    private:
      bool mOwnsMount     = false;
      bool mIsInitialized = false;
  };
}  // namespace doggo::platform::nx
