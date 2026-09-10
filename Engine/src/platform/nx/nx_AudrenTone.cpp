#include "doggo/platform/nx/nx_AudrenTone.hpp"

#include "doggo/platform/nx/nx_MonotonicClock.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <malloc.h>

namespace
{
  constexpr std::uint64_t AudrenPeriodNs    = 5'000'000;
  constexpr std::uint64_t LateWakeupLimitNs = AudrenPeriodNs + ( AudrenPeriodNs / 2 );
  constexpr double        Tau               = 6.28318530717958647692;
  constexpr double        ToneAmplitude     = 0.10;

  [[nodiscard]] constexpr std::size_t alignUp( const std::size_t value, const std::size_t alignment ) noexcept
  {
    return ( value + alignment - 1 ) & ~( alignment - 1 );
  }

  template <typename Type>
  void updateMaximum( std::atomic<Type> & destination, const Type candidate ) noexcept
  {
    Type current = destination.load( std::memory_order_relaxed );
    while ( current < candidate && !destination.compare_exchange_weak( current, candidate, std::memory_order_relaxed ) )
    {
    }
  }

  template <typename Type>
  void updateMinimum( std::atomic<Type> & destination, const Type candidate ) noexcept
  {
    Type current = destination.load( std::memory_order_relaxed );
    while ( current > candidate && !destination.compare_exchange_weak( current, candidate, std::memory_order_relaxed ) )
    {
    }
  }

  [[nodiscard]] std::uint64_t monotonicNanoseconds() noexcept
  {
    return static_cast<std::uint64_t>( doggo::platform::nx::MonotonicClock::now().time_since_epoch().count() );
  }

  [[nodiscard]] constexpr Result invariantFailure() noexcept
  {
    return MAKERESULT( Module_Libnx, LibnxError_BadInput );
  }
}  // namespace

namespace doggo::platform::nx
{
  AudrenTone::~AudrenTone()
  {
    finalize();
  }

  std::uint32_t AudrenTone::initialize() noexcept
  {
    if ( mIsInitialized )
    {
      return 0;
    }

    mConfig = AudioRendererConfig{
        .output_rate     = AudioRendererOutputRate_48kHz,
        .num_voices      = 1,
        .num_effects     = 0,
        .num_sinks       = 1,
        .num_mix_objs    = 1,
        .num_mix_buffers = 2,
    };

    Result result = audrenInitialize( &mConfig );
    if ( R_FAILED( result ) )
    {
      return result;
    }
    mIsAudrenInitialized = true;

    result = audrvCreate( &mDriver, &mConfig, 2 );
    if ( R_FAILED( result ) )
    {
      finalize();
      return result;
    }
    mIsDriverInitialized = true;

    constexpr std::size_t bufferBytes  = SamplesPerBuffer * sizeof( std::int16_t );
    constexpr std::size_t bufferStride = alignUp( bufferBytes, AUDREN_BUFFER_ALIGNMENT );
    mSampleMemorySize                  = alignUp( bufferStride * BufferCount, AUDREN_MEMPOOL_ALIGNMENT );
    mSampleMemory                      = memalign( AUDREN_MEMPOOL_ALIGNMENT, mSampleMemorySize );
    if ( !mSampleMemory )
    {
      result = MAKERESULT( Module_Libnx, LibnxError_OutOfMemory );
      finalize();
      return result;
    }

    prepareToneBuffers();

    const std::int32_t memoryPoolId = audrvMemPoolAdd( &mDriver, mSampleMemory, mSampleMemorySize );
    if ( memoryPoolId < 0 || !audrvMemPoolAttach( &mDriver, memoryPoolId ) )
    {
      result = invariantFailure();
      finalize();
      return result;
    }

    constexpr std::array<std::uint8_t, 2> sinkChannels = { 0, 1 };
    if ( audrvDeviceSinkAdd( &mDriver, AUDREN_DEFAULT_DEVICE_NAME, sinkChannels.size(), sinkChannels.data() ) < 0 )
    {
      result = invariantFailure();
      finalize();
      return result;
    }

    constexpr std::int32_t voiceId = 0;
    if ( !audrvVoiceInit( &mDriver, voiceId, 1, PcmFormat_Int16, SampleRate ) )
    {
      result = invariantFailure();
      finalize();
      return result;
    }

    audrvVoiceSetDestinationMix( &mDriver, voiceId, AUDREN_FINAL_MIX_ID );
    audrvVoiceSetMixFactor( &mDriver, voiceId, 1.0f, 0, 0 );
    audrvVoiceSetMixFactor( &mDriver, voiceId, 1.0f, 0, 1 );

    for ( AudioDriverWaveBuf & waveBuffer : mWaveBuffers )
    {
      if ( !audrvVoiceAddWaveBuf( &mDriver, voiceId, &waveBuffer ) )
      {
        result = invariantFailure();
        finalize();
        return result;
      }
    }
    audrvVoiceStart( &mDriver, voiceId );

    result = audrvUpdate( &mDriver );
    if ( R_FAILED( result ) )
    {
      finalize();
      return result;
    }

    result = audrenStartAudioRenderer();
    if ( R_FAILED( result ) )
    {
      finalize();
      return result;
    }
    mIsRendererStarted = true;

    ueventCreate( &mStopEvent, true );
    result = threadCreate( &mThread, threadEntry, this, nullptr, ThreadStackSize, ThreadPriority, ThreadCore );
    if ( R_FAILED( result ) )
    {
      finalize();
      return result;
    }
    mIsThreadCreated = true;

    result = threadStart( &mThread );
    if ( R_FAILED( result ) )
    {
      finalize();
      return result;
    }
    mIsThreadStarted = true;
    mIsInitialized   = true;
    return 0;
  }

  void AudrenTone::finalize() noexcept
  {
    if ( mIsThreadStarted )
    {
      ueventSignal( &mStopEvent );
      threadWaitForExit( &mThread );
      mIsThreadStarted = false;
    }

    if ( mIsThreadCreated )
    {
      threadClose( &mThread );
      mIsThreadCreated = false;
    }

    if ( mIsRendererStarted )
    {
      audrenStopAudioRenderer();
      mIsRendererStarted = false;
    }

    if ( mIsDriverInitialized )
    {
      audrvClose( &mDriver );
      mIsDriverInitialized = false;
    }

    if ( mIsAudrenInitialized )
    {
      audrenExit();
      mIsAudrenInitialized = false;
    }

    std::free( mSampleMemory );
    mSampleMemory     = nullptr;
    mSampleMemorySize = 0;
    mIsInitialized    = false;
  }

  void AudrenTone::requestPause( const bool shouldPause ) noexcept
  {
    mIsPauseRequested.store( shouldPause, std::memory_order_relaxed );
  }

  void AudrenTone::requestUnderrunTest() noexcept
  {
    mIsUnderrunTestRequested.store( true, std::memory_order_relaxed );
  }

  AudrenTelemetry AudrenTone::telemetry() const noexcept
  {
    return AudrenTelemetry{
        .renderer_frame_count          = mRendererFrameCount.load( std::memory_order_relaxed ),
        .played_sample_count           = mPlayedSampleCount.load( std::memory_order_relaxed ),
        .buffer_underrun_count         = mBufferUnderrunCount.load( std::memory_order_relaxed ),
        .voice_drop_count              = mVoiceDropCount.load( std::memory_order_relaxed ),
        .late_wakeup_count             = mLateWakeupCount.load( std::memory_order_relaxed ),
        .injected_stall_count          = mInjectedStallCount.load( std::memory_order_relaxed ),
        .update_failure_count          = mUpdateFailureCount.load( std::memory_order_relaxed ),
        .wait_failure_count            = mWaitFailureCount.load( std::memory_order_relaxed ),
        .maximum_wakeup_gap_ns         = mMaximumWakeupGapNs.load( std::memory_order_relaxed ),
        .maximum_update_time_ns        = mMaximumUpdateTimeNs.load( std::memory_order_relaxed ),
        .buffered_sample_count         = mBufferedSampleCount.load( std::memory_order_relaxed ),
        .minimum_buffered_sample_count = mMinimumBufferedSampleCount.load( std::memory_order_relaxed ),
        .queued_buffer_count           = mQueuedBufferCount.load( std::memory_order_relaxed ),
        .minimum_buffer_count          = mMinimumBufferCount.load( std::memory_order_relaxed ),
        .last_update_result            = mLastUpdateResult.load( std::memory_order_relaxed ),
        .last_wait_result              = mLastWaitResult.load( std::memory_order_relaxed ),
        .is_paused                     = mIsPaused.load( std::memory_order_relaxed ),
    };
  }

  bool AudrenTone::isInitialized() const noexcept
  {
    return mIsInitialized;
  }

  void AudrenTone::threadEntry( void * const context )
  {
    static_cast<AudrenTone *>( context )->run();
  }

  void AudrenTone::run() noexcept
  {
    const std::array waiters = {
        waiterForUEvent( &mStopEvent ),
        waiterForEvent( audrenGetFrameEvent() ),
    };

    std::uint64_t previousWakeTimeNs = 0;
    while ( true )
    {
      std::int32_t signalledIndex = -1;
      const Result waitResult =
          waitObjects( &signalledIndex, waiters.data(), waiters.size(), std::numeric_limits<std::uint64_t>::max() );
      if ( R_FAILED( waitResult ) )
      {
        mLastWaitResult.store( waitResult, std::memory_order_relaxed );
        mWaitFailureCount.fetch_add( 1, std::memory_order_relaxed );
        break;
      }

      if ( signalledIndex == 0 )
      {
        break;
      }

      if ( signalledIndex != 1 )
      {
        mLastWaitResult.store( invariantFailure(), std::memory_order_relaxed );
        mWaitFailureCount.fetch_add( 1, std::memory_order_relaxed );
        break;
      }

      if ( mIsUnderrunTestRequested.exchange( false, std::memory_order_relaxed ) )
      {
        mInjectedStallCount.fetch_add( 1, std::memory_order_relaxed );
        svcSleepThread( UnderrunTestDurationNs );
      }

      const std::uint64_t wakeTimeNs = monotonicNanoseconds();
      if ( previousWakeTimeNs != 0 )
      {
        const std::uint64_t wakeupGapNs = wakeTimeNs - previousWakeTimeNs;
        updateMaximum( mMaximumWakeupGapNs, wakeupGapNs );
        if ( wakeupGapNs > LateWakeupLimitNs )
        {
          mLateWakeupCount.fetch_add( 1, std::memory_order_relaxed );
        }
      }
      previousWakeTimeNs = wakeTimeNs;

      if ( !serviceRendererFrame( wakeTimeNs ) )
      {
        break;
      }
    }
  }

  bool AudrenTone::serviceRendererFrame( const std::uint64_t wakeTimeNs ) noexcept
  {
    constexpr std::int32_t voiceId = 0;

    const bool shouldPause = mIsPauseRequested.load( std::memory_order_relaxed );
    if ( shouldPause != mIsPaused.load( std::memory_order_relaxed ) )
    {
      audrvVoiceSetPaused( &mDriver, voiceId, shouldPause );
      mIsPaused.store( shouldPause, std::memory_order_relaxed );
    }

    const Result        updateResult = audrvUpdate( &mDriver );
    const std::uint64_t updateTimeNs = monotonicNanoseconds() - wakeTimeNs;
    updateMaximum( mMaximumUpdateTimeNs, updateTimeNs );

    if ( R_FAILED( updateResult ) )
    {
      mLastUpdateResult.store( updateResult, std::memory_order_relaxed );
      mUpdateFailureCount.fetch_add( 1, std::memory_order_relaxed );
      return false;
    }

    mRendererFrameCount.fetch_add( 1, std::memory_order_relaxed );

    const std::uint32_t rawPlayedSamples = audrvVoiceGetPlayedSampleCount( &mDriver, voiceId );
    mExtendedPlayedSampleCount += rawPlayedSamples - mPreviousRawPlayedSamples;
    mPreviousRawPlayedSamples = rawPlayedSamples;
    mPlayedSampleCount.store( mExtendedPlayedSampleCount, std::memory_order_relaxed );
    mVoiceDropCount.store( audrvVoiceGetVoiceDropsCount( &mDriver, voiceId ), std::memory_order_relaxed );

    std::uint64_t bufferedSamplesBeforeRefill = 0;
    if ( mSubmittedSampleCount > mExtendedPlayedSampleCount )
    {
      bufferedSamplesBeforeRefill = mSubmittedSampleCount - mExtendedPlayedSampleCount;
    }
    updateMinimum( mMinimumBufferedSampleCount, bufferedSamplesBeforeRefill );

    const std::uint32_t remainingBufferCount = countQueuedBuffers();
    updateMinimum( mMinimumBufferCount, remainingBufferCount );
    if ( bufferedSamplesBeforeRefill == 0 && !shouldPause )
    {
      mBufferUnderrunCount.fetch_add( 1, std::memory_order_relaxed );
    }

    if ( !queueReleasedBuffers() )
    {
      mLastUpdateResult.store( invariantFailure(), std::memory_order_relaxed );
      mUpdateFailureCount.fetch_add( 1, std::memory_order_relaxed );
      return false;
    }

    mQueuedBufferCount.store( countQueuedBuffers(), std::memory_order_relaxed );
    std::uint64_t bufferedSamplesAfterRefill = 0;
    if ( mSubmittedSampleCount > mExtendedPlayedSampleCount )
    {
      bufferedSamplesAfterRefill = mSubmittedSampleCount - mExtendedPlayedSampleCount;
    }
    mBufferedSampleCount.store( bufferedSamplesAfterRefill, std::memory_order_relaxed );
    return true;
  }

  void AudrenTone::prepareToneBuffers() noexcept
  {
    constexpr std::size_t bufferBytes  = SamplesPerBuffer * sizeof( std::int16_t );
    constexpr std::size_t bufferStride = alignUp( bufferBytes, AUDREN_BUFFER_ALIGNMENT );
    auto * const          sampleMemory = static_cast<std::uint8_t *>( mSampleMemory );

    std::memset( mSampleMemory, 0, mSampleMemorySize );
    for ( std::size_t bufferIndex = 0; bufferIndex < mWaveBuffers.size(); ++bufferIndex )
    {
      auto * const samples = reinterpret_cast<std::int16_t *>( sampleMemory + ( bufferIndex * bufferStride ) );
      for ( std::size_t sampleIndex = 0; sampleIndex < SamplesPerBuffer; ++sampleIndex )
      {
        const std::size_t absoluteSampleIndex = ( bufferIndex * SamplesPerBuffer ) + sampleIndex;
        const double      phase = Tau * ToneFrequency * static_cast<double>( absoluteSampleIndex ) / SampleRate;
        samples[ sampleIndex ] =
            static_cast<std::int16_t>( std::sin( phase ) * ToneAmplitude * std::numeric_limits<std::int16_t>::max() );
      }

      mWaveBuffers[ bufferIndex ] = AudioDriverWaveBuf{
          .data_pcm16          = samples,
          .size                = bufferBytes,
          .start_sample_offset = 0,
          .end_sample_offset   = static_cast<std::int32_t>( SamplesPerBuffer ),
          .context_addr        = nullptr,
          .context_sz          = 0,
          .state               = AudioDriverWaveBufState_Free,
          .is_looping          = false,
          .sequence_id         = 0,
          .next                = nullptr,
      };
    }

    armDCacheFlush( mSampleMemory, mSampleMemorySize );
  }

  std::uint32_t AudrenTone::countQueuedBuffers() const noexcept
  {
    return static_cast<std::uint32_t>(
        std::ranges::count_if( mWaveBuffers,
                               []( const AudioDriverWaveBuf & waveBuffer )
                               {
                                 return waveBuffer.state == AudioDriverWaveBufState_Waiting ||
                                        waveBuffer.state == AudioDriverWaveBufState_Queued ||
                                        waveBuffer.state == AudioDriverWaveBufState_Playing;
                               } ) );
  }

  bool AudrenTone::queueReleasedBuffers() noexcept
  {
    bool didQueueEveryBuffer = true;
    for ( AudioDriverWaveBuf & waveBuffer : mWaveBuffers )
    {
      if ( waveBuffer.state == AudioDriverWaveBufState_Done )
      {
        constexpr std::int32_t voiceId = 0;
        if ( audrvVoiceAddWaveBuf( &mDriver, voiceId, &waveBuffer ) )
        {
          mSubmittedSampleCount += SamplesPerBuffer;
        }
        else
        {
          didQueueEveryBuffer = false;
        }
      }
    }
    return didQueueEveryBuffer;
  }
}  // namespace doggo::platform::nx
