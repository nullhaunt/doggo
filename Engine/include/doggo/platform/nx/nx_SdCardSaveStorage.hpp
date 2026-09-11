#pragma once

#include "doggo/save/save_SaveStorage.hpp"

namespace doggo::platform::nx
{
  // NRO save backend. Installed applications will use a separate Horizon
  // SaveData implementation of the same SaveStorage contract.
  class SdCardSaveStorage final : public save::SaveStorage
  {
    public:
      SdCardSaveStorage() noexcept = default;
      ~SdCardSaveStorage() override;

      [[nodiscard]] std::uint32_t initialize() noexcept override;
      [[nodiscard]] std::uint32_t finalize() noexcept override;

      [[nodiscard]] save::SaveStorageReport readCommittedFile( std::string_view        name,
                                                               std::span<std::uint8_t> destination ) noexcept override;

      [[nodiscard]] save::SaveStorageReport
      writeCommittedFile( std::string_view name, std::span<const std::uint8_t> source ) noexcept override;

      [[nodiscard]] std::string_view backendName() const noexcept override;
      [[nodiscard]] std::string_view rootPath() const noexcept override;
      [[nodiscard]] bool             isInitialized() const noexcept override;

    private:
      bool mOwnsMount     = false;
      bool mIsInitialized = false;
  };
}  // namespace doggo::platform::nx
