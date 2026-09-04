#include "FrameTimer.hpp"

namespace doggo::time
{
    FrameTimer::FrameTimer() noexcept
        : mStartTime( MonotonicClock::now() )
        , mPreviousTime( mStartTime )
    {
    }

    void FrameTimer::tick() noexcept
    {
        const MonotonicClock::time_point currentTime = MonotonicClock::now();
        mDeltaTime    = std::chrono::duration_cast<Duration>( currentTime - mPreviousTime );
        mElapsedTime  = std::chrono::duration_cast<Duration>( currentTime - mStartTime );
        mPreviousTime = currentTime;
        ++mFrameIndex;
    }

    std::uint64_t FrameTimer::getFrameIndex() const noexcept
    {
        return mFrameIndex;
    }

    FrameTimer::Duration FrameTimer::getDeltaTime() const noexcept
    {
        return mDeltaTime;
    }

    FrameTimer::Duration FrameTimer::getElapsedTime() const noexcept
    {
        return mElapsedTime;
    }
} // namespace doggo::time