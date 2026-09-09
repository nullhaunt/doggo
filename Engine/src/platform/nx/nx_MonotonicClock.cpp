#include "doggo/platform/nx/nx_MonotonicClock.hpp"

#include <switch.h>

namespace doggo::platform::nx
{
  MonotonicClock::time_point MonotonicClock::now() noexcept
  {
    const auto nanoseconds = static_cast<rep>( armTicksToNs( armGetSystemTick() ) );
    return time_point{ duration{ nanoseconds } };
  }

  std::uint64_t MonotonicClock::frequency() noexcept
  {
    return armGetSystemTickFreq();
  }
}  // namespace doggo::platform::nx
