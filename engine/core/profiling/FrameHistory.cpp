#include "FrameHistory.hpp"

namespace doggo::profiling
{
    void FrameHistory::push( Duration frameTime ) noexcept
    {
        mSamples[ mNextIndex ] = frameTime;
        mNextIndex             = ( mNextIndex + 1 ) % sCapacity;

        if ( mSampleCount < sCapacity )
        {
            ++mSampleCount;
        }
    }

    std::size_t FrameHistory::getSampleCount() const noexcept
    {
        return mSampleCount;
    }

    FrameHistory::Duration FrameHistory::getLatest() const noexcept
    {
        if ( mSampleCount == 0 )
        {
            return Duration::zero();
        }

        const std::size_t latestIndex = ( mNextIndex + sCapacity - 1 ) % sCapacity;
        return mSamples[ latestIndex ];
    }

    FrameHistory::Duration FrameHistory::getAverage() const noexcept
    {
        if ( mSampleCount == 0 )
        {
            return Duration::zero();
        }

        Duration total = Duration::zero();

        for ( std::size_t i = 0; i < mSampleCount; ++i )
        {
            total += mSamples[ i ];
        }

        return total / mSampleCount;
    }

    FrameHistory::Duration FrameHistory::getMinimum() const noexcept
    {
        if ( mSampleCount == 0 )
        {
            return Duration::zero();
        }

        Duration minimum = mSamples[ 0 ];

        for ( std::size_t i = 1; i < mSampleCount; ++i )
        {
            minimum = std::min( minimum, mSamples[ i ] );
        }

        return minimum;
    }

    FrameHistory::Duration FrameHistory::getMaximum() const noexcept
    {
        if ( mSampleCount == 0 )
        {
            return Duration::zero();
        }

        Duration maximum = mSamples[ 0 ];

        for ( std::size_t i = 1; i < mSampleCount; ++i )
        {
            maximum = std::max( maximum, mSamples[ i ] );
        }

        return maximum;
    }

    double FrameHistory::getAverageFps() const noexcept
    {
        const Duration average = getAverage();
        if ( average <= Duration::zero() )
        {
            return 0.0;
        }

        const std::chrono::duration<double> seconds = average;
        return 1.0 / seconds.count();
    }
} // namespace doggo::profiling
