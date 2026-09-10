#pragma once

#include "doggo/doggo_Macro.hpp"

#include <switch.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace doggo::platform::nx
{
  struct AudrenTelemetry final
  {
      std::uint64_t renderer_frame_count          = 0;
      std::uint64_t played_sample_count           = 0;
      std::uint64_t buffer_underrun_count         = 0;
      std::uint64_t voice_drop_count              = 0;
      std::uint64_t late_wakeup_count             = 0;
      std::uint64_t injected_stall_count          = 0;
      std::uint64_t update_failure_count          = 0;
      std::uint64_t wait_failure_count            = 0;
      std::uint64_t maximum_wakeup_gap_ns         = 0;
      std::uint64_t maximum_update_time_ns        = 0;
      std::uint64_t buffered_sample_count         = 0;
      std::uint64_t minimum_buffered_sample_count = 0;
      std::uint32_t queued_buffer_count           = 0;
      std::uint32_t minimum_buffer_count          = 0;
      std::uint32_t last_update_result            = 0;
      std::uint32_t last_wait_result              = 0;
      bool          is_paused                     = false;
  };

  // Gate 0 audren proof. It owns one diagnostic voice and services audren from
  // the renderer's frame event instead of coupling audio to the application
  // loop. The production mixer will retain this device-thread boundary.
  class AudrenTone final
  {
      DOGGO_DISALLOW_COPY( AudrenTone );
      DOGGO_DISALLOW_MOVE( AudrenTone );

    public:
      static constexpr std::uint32_t SampleRate             = 48'000;
      static constexpr std::uint32_t SamplesPerBuffer       = AUDREN_SAMPLES_PER_FRAME_48KHZ;
      static constexpr std::uint32_t BufferCount            = 4;
      static constexpr std::uint32_t ToneFrequency          = 400;
      static constexpr std::uint64_t UnderrunTestDurationNs = 30'000'000;
      static constexpr std::int32_t  ThreadCore             = 2;
      static constexpr std::int32_t  ThreadPriority         = 0x2B;

      AudrenTone() noexcept = default;
      ~AudrenTone();

      // Returns a libnx Result value. Zero indicates success.
      [[nodiscard]] std::uint32_t initialize() noexcept;
      void                        finalize() noexcept;

      void requestPause( bool shouldPause ) noexcept;
      void requestUnderrunTest() noexcept;

      [[nodiscard]] AudrenTelemetry telemetry() const noexcept;
      [[nodiscard]] bool            isInitialized() const noexcept;

    private:
      static constexpr std::size_t ThreadStackSize = static_cast<std::size_t>( 32 * 1024 );

      static void threadEntry( void * context );

      void               run() noexcept;
      [[nodiscard]] bool serviceRendererFrame( std::uint64_t wakeTimeNs ) noexcept;
      void               prepareToneBuffers() noexcept;

      [[nodiscard]] std::uint32_t countQueuedBuffers() const noexcept;
      [[nodiscard]] bool          queueReleasedBuffers() noexcept;

      AudioRendererConfig                         mConfig           = {};
      AudioDriver                                 mDriver           = {};
      std::array<AudioDriverWaveBuf, BufferCount> mWaveBuffers      = {};
      void *                                      mSampleMemory     = nullptr;
      std::size_t                                 mSampleMemorySize = 0;
      Thread                                      mThread           = {};
      UEvent                                      mStopEvent        = {};

      std::atomic<bool> mIsPauseRequested        = false;
      std::atomic<bool> mIsUnderrunTestRequested = false;

      std::atomic<std::uint64_t> mRendererFrameCount  = 0;
      std::atomic<std::uint64_t> mPlayedSampleCount   = 0;
      std::atomic<std::uint64_t> mBufferUnderrunCount = 0;
      std::atomic<std::uint64_t> mVoiceDropCount      = 0;
      std::atomic<std::uint64_t> mLateWakeupCount     = 0;
      std::atomic<std::uint64_t> mInjectedStallCount  = 0;
      std::atomic<std::uint64_t> mUpdateFailureCount  = 0;
      std::atomic<std::uint64_t> mWaitFailureCount    = 0;
      std::atomic<std::uint64_t> mMaximumWakeupGapNs  = 0;
      std::atomic<std::uint64_t> mMaximumUpdateTimeNs = 0;
      std::atomic<std::uint64_t> mBufferedSampleCount = static_cast<std::uint64_t>( BufferCount * SamplesPerBuffer );
      std::atomic<std::uint64_t> mMinimumBufferedSampleCount =
          static_cast<std::uint64_t>( BufferCount * SamplesPerBuffer );
      std::atomic<std::uint32_t> mQueuedBufferCount  = BufferCount;
      std::atomic<std::uint32_t> mMinimumBufferCount = BufferCount;
      std::atomic<std::uint32_t> mLastUpdateResult   = 0;
      std::atomic<std::uint32_t> mLastWaitResult     = 0;
      std::atomic<bool>          mIsPaused           = false;

      std::uint64_t mSubmittedSampleCount         = static_cast<std::uint64_t>( BufferCount * SamplesPerBuffer );
      std::uint64_t mPendingSubmissionSampleCount = 0;
      std::uint64_t mExtendedPlayedSampleCount    = 0;
      std::uint32_t mPreviousRawPlayedSamples     = 0;

      bool mIsAudrenInitialized = false;
      bool mIsDriverInitialized = false;
      bool mIsRendererStarted   = false;
      bool mIsThreadCreated     = false;
      bool mIsThreadStarted     = false;
      bool mIsInitialized       = false;
  };
}  // namespace doggo::platform::nx
