#include "doggo/gpu/gpu_ArenaAllocator.hpp"

#include <algorithm>

namespace doggo::gpu
{
  bool ArenaAllocationHandle::isValid() const noexcept
  {
    return owner_cookie != 0 && arena_generation != 0 && slot != InvalidSlot && slot_generation != 0;
  }

  bool ArenaAllocation::isValid() const noexcept
  {
    return handle.isValid() && size != 0;
  }

  const char * getArenaAllocationStatusName( const ArenaAllocationStatus status ) noexcept
  {
    switch ( status )
    {
      case ArenaAllocationStatus::Success:
        return "Success";
      case ArenaAllocationStatus::NotInitialized:
        return "Not Initialized";
      case ArenaAllocationStatus::InvalidArgument:
        return "Invalid Argument";
      case ArenaAllocationStatus::SizeOverflow:
        return "Size Overflow";
      case ArenaAllocationStatus::OutOfMemory:
        return "Out of Memory";
      case ArenaAllocationStatus::AllocationLimitReached:
        return "Allocation Limit Reached";
    }

    return "Unknown";
  }

  const char * getArenaReleaseStatusName( const ArenaReleaseStatus status ) noexcept
  {
    switch ( status )
    {
      case ArenaReleaseStatus::Success:
        return "Success";
      case ArenaReleaseStatus::NotInitialized:
        return "Not Initialized";
      case ArenaReleaseStatus::InvalidHandle:
        return "Invalid Handle";
      case ArenaReleaseStatus::ForeignAllocation:
        return "Foreign Allocation";
      case ArenaReleaseStatus::StaleAllocation:
        return "Stale Allocation";
      case ArenaReleaseStatus::AllocationMismatch:
        return "Allocation Mismatch";
      case ArenaReleaseStatus::ReleaseDisabled:
        return "Release Disabled";
    }

    return "Unknown";
  }

  bool ArenaAllocator::initialize( const std::uint32_t capacityBytes ) noexcept
  {
    if ( mIsInitialized || capacityBytes == 0 )
    {
      return false;
    }

    clearState();
    mArenaGeneration = nextGeneration( mArenaGeneration );
    mCapacityBytes   = capacityBytes;
    mFreeRanges[ 0 ] = { .offset = 0, .size = capacityBytes };
    mFreeRangeCount  = 1;
    mIsInitialized   = true;
    return true;
  }

  void ArenaAllocator::finalize() noexcept
  {
    if ( mIsInitialized )
    {
      mArenaGeneration = nextGeneration( mArenaGeneration );
    }

    clearState();
  }

  ArenaAllocationResult ArenaAllocator::allocate( const std::uint32_t size, const std::uint32_t alignment ) noexcept
  {
    incrementSaturated( mAllocationAttemptCount );

    const auto fail = [ this ]( const ArenaAllocationStatus status ) noexcept
    {
      incrementSaturated( mAllocationFailureCount );
      return ArenaAllocationResult{ .status = status };
    };

    if ( !mIsInitialized )
    {
      return fail( ArenaAllocationStatus::NotInitialized );
    }

    if ( size == 0 || alignment == 0 )
    {
      return fail( ArenaAllocationStatus::InvalidArgument );
    }

    std::size_t slotIndex = MaximumAllocationCount;
    for ( std::size_t index = 0; index < mAllocationSlots.size(); ++index )
    {
      if ( !mAllocationSlots[ index ].is_live )
      {
        slotIndex = index;
        break;
      }
    }

    if ( slotIndex == MaximumAllocationCount )
    {
      return fail( ArenaAllocationStatus::AllocationLimitReached );
    }

    std::size_t   bestRangeIndex   = MaximumFreeRangeCount;
    std::uint32_t bestRangeSize    = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t bestOffset       = 0;
    bool          observedOverflow = false;

    for ( std::size_t index = 0; index < mFreeRangeCount; ++index )
    {
      const FreeRange & range         = mFreeRanges[ index ];
      std::uint32_t     alignedOffset = 0;
      if ( !alignUp( range.offset, alignment, alignedOffset ) )
      {
        observedOverflow = true;
        continue;
      }

      const std::uint64_t rangeEnd      = static_cast<std::uint64_t>( range.offset ) + range.size;
      const std::uint64_t allocationEnd = static_cast<std::uint64_t>( alignedOffset ) + size;
      if ( allocationEnd > std::numeric_limits<std::uint32_t>::max() )
      {
        observedOverflow = true;
        continue;
      }

      if ( allocationEnd > rangeEnd )
      {
        continue;
      }

      if ( bestRangeIndex == MaximumFreeRangeCount ||
           range.size < bestRangeSize ||
           ( range.size == bestRangeSize && alignedOffset < bestOffset ) )
      {
        bestRangeIndex = index;
        bestRangeSize  = range.size;
        bestOffset     = alignedOffset;
      }
    }

    if ( bestRangeIndex == MaximumFreeRangeCount )
    {
      return fail( observedOverflow ? ArenaAllocationStatus::SizeOverflow : ArenaAllocationStatus::OutOfMemory );
    }

    const FreeRange     selectedRange = mFreeRanges[ bestRangeIndex ];
    const std::uint32_t prefixSize    = bestOffset - selectedRange.offset;
    const std::uint32_t allocationEnd = bestOffset + size;
    const std::uint32_t rangeEnd      = selectedRange.offset + selectedRange.size;
    const std::uint32_t suffixSize    = rangeEnd - allocationEnd;

    if ( prefixSize != 0 && suffixSize != 0 )
    {
      for ( std::size_t index = mFreeRangeCount; index > bestRangeIndex + 1; --index )
      {
        mFreeRanges[ index ] = mFreeRanges[ index - 1 ];
      }
      mFreeRanges[ bestRangeIndex ]     = { .offset = selectedRange.offset, .size = prefixSize };
      mFreeRanges[ bestRangeIndex + 1 ] = { .offset = allocationEnd, .size = suffixSize };
      ++mFreeRangeCount;
    }
    else if ( prefixSize != 0 )
    {
      mFreeRanges[ bestRangeIndex ].size = prefixSize;
    }
    else if ( suffixSize != 0 )
    {
      mFreeRanges[ bestRangeIndex ] = { .offset = allocationEnd, .size = suffixSize };
    }
    else
    {
      removeFreeRange( bestRangeIndex );
    }

    AllocationSlot & slot = mAllocationSlots[ slotIndex ];
    slot.offset           = bestOffset;
    slot.size             = size;
    slot.is_live          = true;

    mUsedBytes += size;
    mPeakUsedBytes = std::max( mPeakUsedBytes, mUsedBytes );
    ++mLiveAllocationCount;
    mPeakLiveAllocationCount = std::max( mPeakLiveAllocationCount, mLiveAllocationCount );

    return {
        .status = ArenaAllocationStatus::Success,
        .allocation =
            {
                .handle =
                    {
                        .owner_cookie     = reinterpret_cast<std::uintptr_t>( this ),
                        .arena_generation = mArenaGeneration,
                        .slot             = static_cast<std::uint32_t>( slotIndex ),
                        .slot_generation  = slot.generation,
                    },
                .offset = bestOffset,
                .size   = size,
            },
    };
  }

  ArenaReleaseStatus ArenaAllocator::release( const ArenaAllocation & allocation ) noexcept
  {
    const auto fail = [ this ]( const ArenaReleaseStatus status ) noexcept
    {
      incrementSaturated( mInvalidReleaseCount );
      return status;
    };

    if ( !mIsInitialized )
    {
      return fail( ArenaReleaseStatus::NotInitialized );
    }

    if ( !allocation.isValid() || allocation.handle.slot >= MaximumAllocationCount )
    {
      return fail( ArenaReleaseStatus::InvalidHandle );
    }

    if ( allocation.handle.owner_cookie != reinterpret_cast<std::uintptr_t>( this ) )
    {
      return fail( ArenaReleaseStatus::ForeignAllocation );
    }

    if ( allocation.handle.arena_generation != mArenaGeneration )
    {
      return fail( ArenaReleaseStatus::StaleAllocation );
    }

    AllocationSlot & slot = mAllocationSlots[ allocation.handle.slot ];
    if ( !slot.is_live || slot.generation != allocation.handle.slot_generation )
    {
      return fail( ArenaReleaseStatus::StaleAllocation );
    }

    if ( slot.offset != allocation.offset || slot.size != allocation.size )
    {
      return fail( ArenaReleaseStatus::AllocationMismatch );
    }

    insertAndCoalesceFreeRange( { .offset = slot.offset, .size = slot.size } );
    mUsedBytes -= slot.size;
    --mLiveAllocationCount;

    slot.offset     = 0;
    slot.size       = 0;
    slot.generation = nextGeneration( slot.generation );
    slot.is_live    = false;
    return ArenaReleaseStatus::Success;
  }

  bool ArenaAllocator::isInitialized() const noexcept
  {
    return mIsInitialized;
  }

  bool ArenaAllocator::owns( const ArenaAllocation & allocation ) const noexcept
  {
    if ( ( !mIsInitialized ) ||
         ( !allocation.isValid() ) ||
         ( allocation.handle.owner_cookie != reinterpret_cast<std::uintptr_t>( this ) ) ||
         ( allocation.handle.arena_generation != mArenaGeneration ) ||
         ( allocation.handle.slot >= MaximumAllocationCount ) )
    {
      return false;
    }

    const AllocationSlot & slot = mAllocationSlots[ allocation.handle.slot ];
    return slot.is_live &&
           slot.generation == allocation.handle.slot_generation &&
           slot.offset == allocation.offset &&
           slot.size == allocation.size;
  }

  ArenaSnapshot ArenaAllocator::snapshot() const noexcept
  {
    ArenaSnapshot result = {
        .capacity_bytes             = mCapacityBytes,
        .used_bytes                 = mUsedBytes,
        .peak_used_bytes            = mPeakUsedBytes,
        .free_bytes                 = mCapacityBytes - mUsedBytes,
        .free_range_count           = static_cast<std::uint32_t>( mFreeRangeCount ),
        .allocation_capacity        = static_cast<std::uint32_t>( MaximumAllocationCount ),
        .live_allocation_count      = mLiveAllocationCount,
        .peak_live_allocation_count = mPeakLiveAllocationCount,
        .allocation_failure_count   = mAllocationFailureCount,
        .invalid_release_count      = mInvalidReleaseCount,
        .allocation_attempt_count   = mAllocationAttemptCount,
    };

    for ( std::size_t index = 0; index < mFreeRangeCount; ++index )
    {
      result.largest_free_range_bytes = std::max( result.largest_free_range_bytes, mFreeRanges[ index ].size );
    }

    if ( result.free_bytes != 0 )
    {
      const std::uint64_t fragmentedBytes = result.free_bytes - result.largest_free_range_bytes;
      result.fragmentation_per_mille      = static_cast<std::uint32_t>( fragmentedBytes * 1000u / result.free_bytes );
    }

    return result;
  }

  bool ArenaAllocator::alignUp( const std::uint32_t value,
                                const std::uint32_t alignment,
                                std::uint32_t &     alignedValue ) noexcept
  {
    if ( alignment == 0 )
    {
      return false;
    }

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

  std::uint32_t ArenaAllocator::nextGeneration( const std::uint32_t generation ) noexcept
  {
    const std::uint32_t result = generation + 1;
    return result == 0 ? 1 : result;
  }

  void ArenaAllocator::incrementSaturated( std::uint32_t & value ) noexcept
  {
    if ( value != std::numeric_limits<std::uint32_t>::max() )
    {
      ++value;
    }
  }

  void ArenaAllocator::incrementSaturated( std::uint64_t & value ) noexcept
  {
    if ( value != std::numeric_limits<std::uint64_t>::max() )
    {
      ++value;
    }
  }

  void ArenaAllocator::removeFreeRange( const std::size_t index ) noexcept
  {
    for ( std::size_t nextIndex = index + 1; nextIndex < mFreeRangeCount; ++nextIndex )
    {
      mFreeRanges[ nextIndex - 1 ] = mFreeRanges[ nextIndex ];
    }

    --mFreeRangeCount;
    mFreeRanges[ mFreeRangeCount ] = {};
  }

  void ArenaAllocator::insertAndCoalesceFreeRange( const FreeRange range ) noexcept
  {
    std::size_t insertionIndex = 0;
    while ( insertionIndex < mFreeRangeCount && mFreeRanges[ insertionIndex ].offset < range.offset )
    {
      ++insertionIndex;
    }

    if ( insertionIndex != 0 )
    {
      FreeRange & previous = mFreeRanges[ insertionIndex - 1 ];
      if ( previous.offset + previous.size == range.offset )
      {
        previous.size += range.size;
        if ( insertionIndex < mFreeRangeCount &&
             previous.offset + previous.size == mFreeRanges[ insertionIndex ].offset )
        {
          previous.size += mFreeRanges[ insertionIndex ].size;
          removeFreeRange( insertionIndex );
        }
        return;
      }
    }

    if ( insertionIndex < mFreeRangeCount && range.offset + range.size == mFreeRanges[ insertionIndex ].offset )
    {
      mFreeRanges[ insertionIndex ].offset = range.offset;
      mFreeRanges[ insertionIndex ].size += range.size;
      return;
    }

    for ( std::size_t index = mFreeRangeCount; index > insertionIndex; --index )
    {
      mFreeRanges[ index ] = mFreeRanges[ index - 1 ];
    }
    mFreeRanges[ insertionIndex ] = range;
    ++mFreeRangeCount;
  }

  void ArenaAllocator::clearState() noexcept
  {
    mFreeRanges = {};
    for ( AllocationSlot & slot : mAllocationSlots )
    {
      slot = {};
    }
    mFreeRangeCount          = 0;
    mCapacityBytes           = 0;
    mUsedBytes               = 0;
    mPeakUsedBytes           = 0;
    mLiveAllocationCount     = 0;
    mPeakLiveAllocationCount = 0;
    mAllocationFailureCount  = 0;
    mInvalidReleaseCount     = 0;
    mAllocationAttemptCount  = 0;
    mIsInitialized           = false;
  }
}  // namespace doggo::gpu
