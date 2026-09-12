#pragma once

#include "doggo/doggo_Macro.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace doggo::gpu
{
  struct ArenaAllocationHandle final
  {
      static constexpr std::uint32_t InvalidSlot = std::numeric_limits<std::uint32_t>::max();

      std::uintptr_t owner_cookie     = 0;
      std::uint32_t  arena_generation = 0;
      std::uint32_t  slot             = InvalidSlot;
      std::uint32_t  slot_generation  = 0;

      [[nodiscard]] bool isValid() const noexcept;

      constexpr bool operator==( const ArenaAllocationHandle & ) const noexcept = default;
  };

  struct ArenaAllocation final
  {
      ArenaAllocationHandle handle = {};
      std::uint32_t         offset = 0;
      std::uint32_t         size   = 0;

      [[nodiscard]] bool isValid() const noexcept;

      constexpr bool operator==( const ArenaAllocation & ) const noexcept = default;
  };

  enum class ArenaAllocationStatus : std::uint8_t
  {
    Success,
    NotInitialized,
    InvalidArgument,
    SizeOverflow,
    OutOfMemory,
    AllocationLimitReached,
  };

  enum class ArenaReleaseStatus : std::uint8_t
  {
    Success,
    NotInitialized,
    InvalidHandle,
    ForeignAllocation,
    StaleAllocation,
    AllocationMismatch,
    ReleaseDisabled,
  };

  struct ArenaAllocationResult final
  {
      ArenaAllocationStatus status     = ArenaAllocationStatus::NotInitialized;
      ArenaAllocation       allocation = {};
  };

  struct ArenaSnapshot final
  {
      std::uint32_t capacity_bytes             = 0;
      std::uint32_t used_bytes                 = 0;
      std::uint32_t peak_used_bytes            = 0;
      std::uint32_t free_bytes                 = 0;
      std::uint32_t largest_free_range_bytes   = 0;
      std::uint32_t free_range_count           = 0;
      std::uint32_t allocation_capacity        = 0;
      std::uint32_t live_allocation_count      = 0;
      std::uint32_t peak_live_allocation_count = 0;
      std::uint32_t allocation_failure_count   = 0;
      std::uint32_t invalid_release_count      = 0;
      std::uint32_t fragmentation_per_mille    = 0;
      std::uint64_t allocation_attempt_count   = 0;
  };

  [[nodiscard]] const char * getArenaAllocationStatusName( ArenaAllocationStatus status ) noexcept;
  [[nodiscard]] const char * getArenaReleaseStatusName( ArenaReleaseStatus status ) noexcept;

  class ArenaAllocator final
  {
      DOGGO_DISALLOW_COPY( ArenaAllocator );
      DOGGO_DISALLOW_MOVE( ArenaAllocator );

    public:
      // Metadata is owned inline: allocation, release, and snapshot never allocate from the CPU heap.
      // One owning subsystem may use an allocator; external synchronization is required across threads.
      static constexpr std::size_t MaximumAllocationCount = 4096;
      static constexpr std::size_t MaximumFreeRangeCount  = MaximumAllocationCount + 1;

      ArenaAllocator() noexcept = default;

      [[nodiscard]] bool initialize( std::uint32_t capacityBytes ) noexcept;
      void               finalize() noexcept;

      [[nodiscard]] ArenaAllocationResult allocate( std::uint32_t size, std::uint32_t alignment ) noexcept;
      [[nodiscard]] ArenaReleaseStatus    release( const ArenaAllocation & allocation ) noexcept;

      [[nodiscard]] bool          isInitialized() const noexcept;
      [[nodiscard]] bool          owns( const ArenaAllocation & allocation ) const noexcept;
      [[nodiscard]] ArenaSnapshot snapshot() const noexcept;

    private:
      struct FreeRange final
      {
          std::uint32_t offset = 0;
          std::uint32_t size   = 0;
      };

      struct AllocationSlot final
      {
          std::uint32_t offset     = 0;
          std::uint32_t size       = 0;
          std::uint32_t generation = 1;
          bool          is_live    = false;
      };

      [[nodiscard]] static bool          alignUp( std::uint32_t value, std::uint32_t alignment,
                                                  std::uint32_t & alignedValue ) noexcept;
      [[nodiscard]] static std::uint32_t nextGeneration( std::uint32_t generation ) noexcept;
      static void                        incrementSaturated( std::uint32_t & value ) noexcept;
      static void                        incrementSaturated( std::uint64_t & value ) noexcept;

      void removeFreeRange( std::size_t index ) noexcept;
      void insertAndCoalesceFreeRange( FreeRange range ) noexcept;
      void clearState() noexcept;

      std::array<FreeRange, MaximumFreeRangeCount>       mFreeRanges              = {};
      std::array<AllocationSlot, MaximumAllocationCount> mAllocationSlots         = {};
      std::size_t                                        mFreeRangeCount          = 0;
      std::uint32_t                                      mCapacityBytes           = 0;
      std::uint32_t                                      mUsedBytes               = 0;
      std::uint32_t                                      mPeakUsedBytes           = 0;
      std::uint32_t                                      mLiveAllocationCount     = 0;
      std::uint32_t                                      mPeakLiveAllocationCount = 0;
      std::uint32_t                                      mAllocationFailureCount  = 0;
      std::uint32_t                                      mInvalidReleaseCount     = 0;
      std::uint32_t                                      mArenaGeneration         = 0;
      std::uint64_t                                      mAllocationAttemptCount  = 0;
      bool                                               mIsInitialized           = false;
  };
}  // namespace doggo::gpu
