#pragma once

#include <array>
#include <span>

#include "time/Clock.hpp"

namespace doggo::profiling
{
    struct CpuSample
    {
        std::string_view         mName;
        std::chrono::nanoseconds mDuration = {};
        std::uint32_t            mDepth    = 0;
    };

    class CpuScope;

    class CpuProfiler final
    {
      public:
        static constexpr std::size_t sMaxSamplesPerFrame = 64;

        void beginFrame() noexcept;

        [[nodiscard]] std::span<const CpuSample> getLastFrameSamples() const noexcept;
        [[nodiscard]] bool                       hasLastFrameOverflowed() const noexcept;

      private:
        friend class CpuScope;

        static constexpr std::size_t sInvalidSampleIndex = static_cast<std::size_t>( -1 );

        struct Buffer
        {
            std::array<CpuSample, sMaxSamplesPerFrame> mSamples       = {};
            std::size_t                                mCount         = 0;
            bool                                       mHasOverflowed = false;
        };

        void beginScope( std::string_view name, std::size_t & sampleIndex, std::uint32_t & depth ) noexcept;
        void endScope( std::size_t sampleIndex, std::uint32_t depth, std::chrono::nanoseconds duration ) noexcept;

        std::array<Buffer, 2> mBuffers         = {};
        std::size_t           mCurrentBuffer   = 0;
        std::size_t           mCompletedBuffer = 1;
        std::uint32_t         mCurrentDepth    = 0;

        bool mHasFrameStarted = false;
        bool mIsFrameComplete = false;
    };

    class CpuScope final
    {
      public:
        CpuScope( CpuProfiler & profiler, std::string_view name ) noexcept;
        ~CpuScope();

        CpuScope( const CpuScope & )             = delete;
        CpuScope & operator=( const CpuScope & ) = delete;

        CpuScope( CpuScope && )             = delete;
        CpuScope & operator=( CpuScope && ) = delete;

      private:
        CpuProfiler *                    mProfiler  = nullptr;
        time::MonotonicClock::time_point mStartTime = {};

        std::size_t   mSampleIndex = CpuProfiler::sInvalidSampleIndex;
        std::uint32_t mDepth       = 0;
    };
} // namespace doggo::profiling
