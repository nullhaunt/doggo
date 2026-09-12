#include "doggo/gpu/gpu_UploadRingAllocator.hpp"

#include <algorithm>

namespace doggo::gpu
{
  bool UploadRingSlice::isValid() const noexcept
  {
    return owner_cookie != 0 &&
           ring_generation != 0 &&
           frame_index != std::numeric_limits<std::uint32_t>::max() &&
           frame_generation != 0 &&
           size != 0;
  }

  const char * getUploadRingStatusName( const UploadRingStatus status ) noexcept
  {
    switch ( status )
    {
      case UploadRingStatus::Success:
        return "Success";

      case UploadRingStatus::NotInitialized:
        return "Not Initialized";

      case UploadRingStatus::InvalidArgument:
        return "Invalid Argument";

      case UploadRingStatus::FrameAlreadyActive:
        return "Frame Already Active";

      case UploadRingStatus::NoActiveFrame:
        return "No Active Frame";

      case UploadRingStatus::FrameIndexOutOfRange:
        return "Frame Index Out of Range";

      case UploadRingStatus::FrameRetirementOutOfOrder:
        return "Frame Retirement Out of Order";

      case UploadRingStatus::SizeOverflow:
        return "Size Overflow";

      case UploadRingStatus::OutOfSpace:
        return "Out of Space";
    }

    return "Unknown";
  }

  bool UploadRingAllocator::initialize( const std::uint32_t capacityBytes, const std::uint32_t frameCount ) noexcept
  {
    if ( mIsInitialized || capacityBytes == 0 || frameCount == 0 || frameCount > MaximumFrameCount )
    {
      return false;
    }

    clearState();
    mRingGeneration = nextGeneration( mRingGeneration );
    mCapacityBytes  = capacityBytes;
    mFrameCount     = frameCount;
    mIsInitialized  = true;
    return true;
  }

  void UploadRingAllocator::finalize() noexcept
  {
    if ( mIsInitialized )
    {
      mRingGeneration = nextGeneration( mRingGeneration );
    }

    clearState();
  }

  UploadRingStatus UploadRingAllocator::beginFrame( const std::uint32_t frameIndex,
                                                    const bool          waitedForCompletion ) noexcept
  {
    if ( !mIsInitialized )
    {
      return UploadRingStatus::NotInitialized;
    }

    if ( mActiveFrameIndex != InvalidFrameIndex )
    {
      return UploadRingStatus::FrameAlreadyActive;
    }

    if ( frameIndex >= mFrameCount )
    {
      return UploadRingStatus::FrameIndexOutOfRange;
    }

    FrameState & frame = mFrames[ frameIndex ];
    if ( frame.is_in_flight )
    {
      for ( std::uint32_t index = 0; index < mFrameCount; ++index )
      {
        const FrameState & other = mFrames[ index ];
        if ( other.is_in_flight && other.retire_position < frame.retire_position )
        {
          return UploadRingStatus::FrameRetirementOutOfOrder;
        }
      }

      if ( waitedForCompletion && frame.retire_position > mTailPosition )
      {
        incrementSaturated( mStallCount );
      }

      mTailPosition      = frame.retire_position;
      frame.is_in_flight = false;
      incrementSaturated( mRetiredFrameCount );
    }

    frame.generation       = nextGeneration( frame.generation );
    frame.start_position   = mHeadPosition;
    frame.retire_position  = mHeadPosition;
    frame.is_active        = true;
    mActiveFrameIndex      = frameIndex;
    mCurrentFrameBytes     = 0;
    mCurrentFrameWrapCount = 0;

    return UploadRingStatus::Success;
  }

  UploadRingAllocationResult UploadRingAllocator::allocate( const std::uint32_t size,
                                                            const std::uint32_t alignment ) noexcept
  {
    incrementSaturated( mAllocationAttemptCount );

    const auto fail = [ this ]( const UploadRingStatus status ) noexcept
    {
      incrementSaturated( mAllocationFailureCount );
      return UploadRingAllocationResult{ .status = status };
    };

    if ( !mIsInitialized )
    {
      return fail( UploadRingStatus::NotInitialized );
    }

    if ( mActiveFrameIndex == InvalidFrameIndex )
    {
      return fail( UploadRingStatus::NoActiveFrame );
    }

    if ( size == 0 || alignment == 0 )
    {
      return fail( UploadRingStatus::InvalidArgument );
    }

    if ( size > mCapacityBytes )
    {
      return fail( UploadRingStatus::OutOfSpace );
    }

    const std::uint64_t occupiedBytes    = mHeadPosition - mTailPosition;
    const std::uint64_t availableBytes   = mCapacityBytes - occupiedBytes;
    const auto          physicalPosition = static_cast<std::uint32_t>( mHeadPosition % mCapacityBytes );

    std::uint32_t alignedPosition = 0;
    if ( !alignUp( physicalPosition, alignment, alignedPosition ) )
    {
      return fail( UploadRingStatus::SizeOverflow );
    }

    std::uint64_t paddingBytes = alignedPosition - physicalPosition;
    bool          didWrap      = false;
    if ( static_cast<std::uint64_t>( alignedPosition ) + size > mCapacityBytes )
    {
      paddingBytes    = mCapacityBytes - physicalPosition;
      alignedPosition = 0;
      didWrap         = true;
    }

    const std::uint64_t consumedBytes = paddingBytes + size;
    if ( consumedBytes > availableBytes || mHeadPosition > std::numeric_limits<std::uint64_t>::max() - consumedBytes )
    {
      return fail( mHeadPosition > std::numeric_limits<std::uint64_t>::max() - consumedBytes
                       ? UploadRingStatus::SizeOverflow
                       : UploadRingStatus::OutOfSpace );
    }

    const std::uint64_t virtualPosition = mHeadPosition + paddingBytes;
    mHeadPosition += consumedBytes;
    mCurrentFrameBytes += size;
    mPeakFrameBytes     = std::max( mPeakFrameBytes, mCurrentFrameBytes );
    mPeakOccupancyBytes = std::max( mPeakOccupancyBytes, static_cast<std::uint32_t>( mHeadPosition - mTailPosition ) );

    if ( didWrap )
    {
      incrementSaturated( mWrapCount );
      incrementSaturated( mCurrentFrameWrapCount );
    }

    const FrameState & frame = mFrames[ mActiveFrameIndex ];
    return {
        .status = UploadRingStatus::Success,
        .slice =
            {
                .owner_cookie     = reinterpret_cast<std::uintptr_t>( this ),
                .virtual_position = virtualPosition,
                .ring_generation  = mRingGeneration,
                .frame_index      = mActiveFrameIndex,
                .frame_generation = frame.generation,
                .offset           = alignedPosition,
                .size             = size,
            },
    };
  }

  UploadRingStatus UploadRingAllocator::endFrame() noexcept
  {
    if ( !mIsInitialized )
    {
      return UploadRingStatus::NotInitialized;
    }

    if ( mActiveFrameIndex == InvalidFrameIndex )
    {
      return UploadRingStatus::NoActiveFrame;
    }

    FrameState & frame    = mFrames[ mActiveFrameIndex ];
    frame.retire_position = mHeadPosition;
    frame.is_active       = false;
    frame.is_in_flight    = true;

    mLastFrameBytes = mCurrentFrameBytes;
    addSaturated( mTotalSubmittedBytes, mCurrentFrameBytes );
    incrementSaturated( mSubmittedFrameCount );
    mCurrentFrameBytes     = 0;
    mCurrentFrameWrapCount = 0;
    mActiveFrameIndex      = InvalidFrameIndex;
    return UploadRingStatus::Success;
  }

  UploadRingStatus UploadRingAllocator::abortFrame() noexcept
  {
    if ( !mIsInitialized )
    {
      return UploadRingStatus::NotInitialized;
    }

    if ( mActiveFrameIndex == InvalidFrameIndex )
    {
      return UploadRingStatus::NoActiveFrame;
    }

    FrameState & frame = mFrames[ mActiveFrameIndex ];
    mHeadPosition      = frame.start_position;
    frame.is_active    = false;
    if ( mWrapCount != std::numeric_limits<std::uint64_t>::max() )
    {
      mWrapCount -= mCurrentFrameWrapCount;
    }
    mCurrentFrameBytes     = 0;
    mCurrentFrameWrapCount = 0;
    mActiveFrameIndex      = InvalidFrameIndex;
    return UploadRingStatus::Success;
  }

  UploadRingStatus UploadRingAllocator::retireAllFrames() noexcept
  {
    if ( !mIsInitialized )
    {
      return UploadRingStatus::NotInitialized;
    }

    if ( mActiveFrameIndex != InvalidFrameIndex )
    {
      return UploadRingStatus::FrameAlreadyActive;
    }

    for ( std::uint32_t index = 0; index < mFrameCount; ++index )
    {
      FrameState & frame = mFrames[ index ];
      if ( frame.is_in_flight )
      {
        frame.is_in_flight = false;
        incrementSaturated( mRetiredFrameCount );
      }
    }

    mTailPosition = mHeadPosition;
    return UploadRingStatus::Success;
  }

  bool UploadRingAllocator::isInitialized() const noexcept
  {
    return mIsInitialized;
  }

  bool UploadRingAllocator::owns( const UploadRingSlice & slice ) const noexcept
  {
    if ( !mIsInitialized ||
         !slice.isValid() ||
         slice.owner_cookie != reinterpret_cast<std::uintptr_t>( this ) ||
         slice.ring_generation != mRingGeneration ||
         slice.frame_index >= mFrameCount ||
         slice.offset >= mCapacityBytes ||
         slice.size > mCapacityBytes - slice.offset ||
         slice.virtual_position % mCapacityBytes != slice.offset )
    {
      return false;
    }

    const FrameState & frame = mFrames[ slice.frame_index ];
    if ( frame.generation != slice.frame_generation || ( !frame.is_active && !frame.is_in_flight ) )
    {
      return false;
    }

    const std::uint64_t frameEnd = frame.is_active ? mHeadPosition : frame.retire_position;
    return slice.virtual_position >= frame.start_position &&
           slice.virtual_position <= frameEnd &&
           slice.size <= frameEnd - slice.virtual_position;
  }

  UploadRingSnapshot UploadRingAllocator::snapshot() const noexcept
  {
    const auto    occupancyBytes     = static_cast<std::uint32_t>( mHeadPosition - mTailPosition );
    std::uint32_t inFlightFrameCount = 0;
    for ( std::uint32_t index = 0; index < mFrameCount; ++index )
    {
      if ( mFrames[ index ].is_in_flight )
      {
        ++inFlightFrameCount;
      }
    }

    return {
        .capacity_bytes           = mCapacityBytes,
        .occupancy_bytes          = occupancyBytes,
        .peak_occupancy_bytes     = mPeakOccupancyBytes,
        .available_bytes          = mCapacityBytes - occupancyBytes,
        .current_frame_bytes      = mCurrentFrameBytes,
        .last_frame_bytes         = mLastFrameBytes,
        .peak_frame_bytes         = mPeakFrameBytes,
        .configured_frame_count   = mFrameCount,
        .in_flight_frame_count    = inFlightFrameCount,
        .allocation_failure_count = mAllocationFailureCount,
        .allocation_attempt_count = mAllocationAttemptCount,
        .submitted_frame_count    = mSubmittedFrameCount,
        .retired_frame_count      = mRetiredFrameCount,
        .total_submitted_bytes    = mTotalSubmittedBytes,
        .wrap_count               = mWrapCount,
        .stall_count              = mStallCount,
        .has_active_frame         = mActiveFrameIndex != InvalidFrameIndex,
    };
  }

  bool UploadRingAllocator::alignUp( const std::uint32_t value,
                                     const std::uint32_t alignment,
                                     std::uint32_t &     alignedValue ) noexcept
  {
    const std::uint32_t remainder = value % alignment;
    if ( remainder == 0 )
    {
      alignedValue = value;
      return true;
    }

    const std::uint64_t result = static_cast<std::uint64_t>( value ) + alignment - remainder;
    if ( result > std::numeric_limits<std::uint32_t>::max() )
    {
      return false;
    }

    alignedValue = static_cast<std::uint32_t>( result );
    return true;
  }

  std::uint32_t UploadRingAllocator::nextGeneration( const std::uint32_t generation ) noexcept
  {
    const std::uint32_t result = generation + 1;
    return result == 0 ? 1 : result;
  }

  void UploadRingAllocator::addSaturated( std::uint64_t & value, const std::uint64_t amount ) noexcept
  {
    value = amount > std::numeric_limits<std::uint64_t>::max() - value ? std::numeric_limits<std::uint64_t>::max()
                                                                       : value + amount;
  }

  void UploadRingAllocator::incrementSaturated( std::uint32_t & value ) noexcept
  {
    if ( value != std::numeric_limits<std::uint32_t>::max() )
    {
      ++value;
    }
  }

  void UploadRingAllocator::incrementSaturated( std::uint64_t & value ) noexcept
  {
    if ( value != std::numeric_limits<std::uint64_t>::max() )
    {
      ++value;
    }
  }

  void UploadRingAllocator::clearState() noexcept
  {
    mFrames                 = {};
    mHeadPosition           = 0;
    mTailPosition           = 0;
    mAllocationAttemptCount = 0;
    mSubmittedFrameCount    = 0;
    mRetiredFrameCount      = 0;
    mTotalSubmittedBytes    = 0;
    mWrapCount              = 0;
    mStallCount             = 0;
    mCurrentFrameWrapCount  = 0;
    mCapacityBytes          = 0;
    mPeakOccupancyBytes     = 0;
    mCurrentFrameBytes      = 0;
    mLastFrameBytes         = 0;
    mPeakFrameBytes         = 0;
    mFrameCount             = 0;
    mActiveFrameIndex       = InvalidFrameIndex;
    mAllocationFailureCount = 0;
    mIsInitialized          = false;
  }
}  // namespace doggo::gpu
