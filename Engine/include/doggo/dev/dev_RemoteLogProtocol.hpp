#pragma once

#include <array>
#include <cstdint>

namespace doggo::dev
{
  inline constexpr std::uint16_t NetloaderPort = 28'280;
  inline constexpr std::uint16_t RemoteLogPort = 28'771;

  inline constexpr int RemoteLogAttachTimeoutMilliseconds = 10'000;

  // Raw log text follows this preamble. Byte 5 is the protocol version.
  inline constexpr std::array<char, 8> RemoteLogPreamble{ 'D', 'O', 'G', 'G', 'O', 1, '\r', '\n' };
}  // namespace doggo::dev
