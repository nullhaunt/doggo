#pragma once

#include "Clock.hpp"

namespace doggo::time
{
    class FrameTimer final
    {
      public:
        using Duration = std::chrono::nanoseconds;

        FrameTimer() noexcept;

        void tick() noexcept;

        [[nodiscard]] std::uint64_t getFrameIndex() const noexcept;
        [[nodiscard]] Duration      getDeltaTime() const noexcept;
        [[nodiscard]] Duration      getElapsedTime() const noexcept;

      private:
        MonotonicClock::time_point mStartTime;
        MonotonicClock::time_point mPreviousTime;

        std::uint64_t mFrameIndex = 0;

        Duration mDeltaTime   = Duration::zero();
        Duration mElapsedTime = Duration::zero();
    };
} // namespace doggo::time
