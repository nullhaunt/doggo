#pragma once

#include <array>
#include <chrono>

namespace doggo::profiling
{
    class FrameHistory final
    {
      public:
        using Duration = std::chrono::nanoseconds;

        static constexpr std::size_t sCapacity = 120;

        void push( Duration frameTime ) noexcept;

        [[nodiscard]] std::size_t getSampleCount() const noexcept;
        [[nodiscard]] Duration    getLatest() const noexcept;
        [[nodiscard]] Duration    getAverage() const noexcept;
        [[nodiscard]] Duration    getMinimum() const noexcept;
        [[nodiscard]] Duration    getMaximum() const noexcept;
        [[nodiscard]] double      getAverageFps() const noexcept;

      private:
        std::array<Duration, sCapacity> mSamples     = {};
        std::size_t                     mNextIndex   = 0;
        std::size_t                     mSampleCount = 0;
    };
} // namespace doggo::profiling
