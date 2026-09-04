#include "CpuProfiler.hpp"

#include "diagnostics/Assert.hpp"

namespace doggo::profiling
{
    void CpuProfiler::beginFrame() noexcept
    {
        DOGGO_ASSERT( mCurrentDepth == 0 );

        if ( mHasFrameStarted )
        {
            mCompletedBuffer = mCurrentBuffer;
            mCurrentBuffer   = 1 - mCurrentBuffer;
            mIsFrameComplete = true;
        }
        else
        {
            mHasFrameStarted = true;
        }

        Buffer & current       = mBuffers.at( mCurrentDepth );
        current.mCount         = 0;
        current.mHasOverflowed = false;
        mCurrentDepth          = 0;
    }

    std::span<const CpuSample> CpuProfiler::getLastFrameSamples() const noexcept
    {
        if ( !mIsFrameComplete )
        {
            return {};
        }

        const Buffer & completed = mBuffers.at( mCompletedBuffer );
        return { completed.mSamples.data(), completed.mCount };
    }

    bool CpuProfiler::hasLastFrameOverflowed() const noexcept
    {
        if ( !mIsFrameComplete )
        {
            return false;
        }

        return mBuffers.at( mCompletedBuffer ).mHasOverflowed;
    }

    void CpuProfiler::beginScope( const std::string_view name,
                                  std::size_t &          sampleIndex,
                                  std::uint32_t &        depth ) noexcept
    {
        DOGGO_ASSERT( mHasFrameStarted );

        depth = mCurrentDepth;
        ++mCurrentDepth;

        Buffer & current = mBuffers.at( mCurrentBuffer );
        if ( current.mCount >= sMaxSamplesPerFrame )
        {
            current.mHasOverflowed = true;
            sampleIndex            = sInvalidSampleIndex;
            return;
        }

        sampleIndex = current.mCount++;

        CpuSample & sample = current.mSamples.at( sampleIndex );
        sample.mName       = name;
        sample.mDuration   = std::chrono::nanoseconds::zero();
        sample.mDepth      = depth;
    }

    void CpuProfiler::endScope( const std::size_t              sampleIndex,
                                const std::uint32_t            depth,
                                const std::chrono::nanoseconds duration ) noexcept
    {
        DOGGO_ASSERT( mCurrentDepth > 0 );
        DOGGO_ASSERT( mCurrentDepth == depth + 1 );

        mCurrentDepth = depth;

        if ( sampleIndex == sInvalidSampleIndex )
        {
            return;
        }

        Buffer & current = mBuffers.at( mCurrentBuffer );
        DOGGO_ASSERT( sampleIndex < current.mCount );

        current.mSamples.at( sampleIndex ).mDuration = duration;
    }

    CpuScope::CpuScope( CpuProfiler & profiler, std::string_view name ) noexcept
        : mProfiler( &profiler )
    {
        mProfiler->beginScope( name, mSampleIndex, mDepth );
        mStartTime = time::MonotonicClock::now();
    }

    CpuScope::~CpuScope()
    {
        const auto endTime  = time::MonotonicClock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>( endTime - mStartTime );
        mProfiler->endScope( mSampleIndex, mDepth, duration );
    }
} // namespace doggo::profiling
