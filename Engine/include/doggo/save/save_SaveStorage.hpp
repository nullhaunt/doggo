#pragma once

#include "doggo/doggo_Macro.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace doggo::save
{
  enum class SaveStorageStatus : std::uint8_t
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

  struct SaveStorageReport final
  {
      SaveStorageStatus status            = SaveStorageStatus::NotInitialized;
      std::uint32_t     native_result     = 0;
      int               error_number      = 0;
      std::size_t       file_size         = 0;
      std::size_t       bytes_transferred = 0;
  };

  // Backend-neutral boundary for durable save storage. Serialization,
  // validation, migration, and recovery remain above this interface; each
  // backend owns its platform-specific flush and commit requirements.
  class SaveStorage
  {
      DOGGO_DISALLOW_COPY( SaveStorage );
      DOGGO_DISALLOW_MOVE( SaveStorage );

    public:
      SaveStorage() noexcept = default;
      virtual ~SaveStorage() = default;

      // Native result value. Zero indicates success.
      [[nodiscard]] virtual std::uint32_t initialize() noexcept = 0;
      [[nodiscard]] virtual std::uint32_t finalize() noexcept   = 0;

      [[nodiscard]] virtual SaveStorageReport readCommittedFile( std::string_view        name,
                                                                 std::span<std::uint8_t> destination ) noexcept = 0;

      [[nodiscard]] virtual SaveStorageReport writeCommittedFile( std::string_view              name,
                                                                  std::span<const std::uint8_t> source ) noexcept = 0;

      [[nodiscard]] virtual std::string_view backendName() const noexcept   = 0;
      [[nodiscard]] virtual std::string_view rootPath() const noexcept      = 0;
      [[nodiscard]] virtual bool             isInitialized() const noexcept = 0;
  };
}  // namespace doggo::save
