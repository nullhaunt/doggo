#pragma once

#include <chrono>

namespace doggo::time
{
    using MonotonicClock = std::chrono::steady_clock;
    static_assert( MonotonicClock::is_steady, "DOGGO requires a monotonic steady clock." );
} // namespace doggo::time
