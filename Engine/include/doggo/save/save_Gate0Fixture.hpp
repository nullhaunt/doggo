#pragma once

#include "save_SaveStorage.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace doggo::save
{
  enum class Gate0FixtureStatus : std::uint8_t
  {
    Success,
    StorageInitializationFailed,
    PreviousReadFailed,
    PreviousFixtureInvalid,
    GenerationExhausted,
    WriteFailed,
    ReadBackFailed,
    ReadBackInvalid,
  };

  struct Gate0FixtureReport final
  {
      Gate0FixtureStatus status = Gate0FixtureStatus::StorageInitializationFailed;
      SaveStorageReport  storage;
      bool               had_previous_fixture = false;
      std::uint64_t      previous_generation  = 0;
      std::uint64_t      committed_generation = 0;
      std::uint64_t      content_hash         = 0;
  };

  inline constexpr std::string_view Gate0FixtureName = "gate0_save_fixture.bin";
  inline constexpr std::size_t      Gate0FixtureSize = 64;

  // Proves creation/read-back on first run and cross-launch persistence on
  // subsequent runs. Invalid existing data is reported without overwriting it.
  [[nodiscard]] Gate0FixtureReport runGate0Fixture( SaveStorage & storage ) noexcept;
}  // namespace doggo::save
