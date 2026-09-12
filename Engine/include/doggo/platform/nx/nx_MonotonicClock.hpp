#pragma once

#include "doggo/doggo_Macro.hpp"

#include <chrono>
#include <ratio>

namespace doggo::platform::nx
{
  class MonotonicClock final
  {
      DOGGO_DISALLOW_COPY( MonotonicClock );
      DOGGO_DISALLOW_MOVE( MonotonicClock );

    public:
      using rep        = std::int64_t;
      using period     = std::nano;
      using duration   = std::chrono::nanoseconds;
      using time_point = std::chrono::time_point<MonotonicClock, duration>;

      static constexpr bool IsSteady = true;

      [[nodiscard]] static time_point    now() noexcept;
      [[nodiscard]] static std::uint64_t frequency() noexcept;
  };
}  // namespace doggo::platform::nx
