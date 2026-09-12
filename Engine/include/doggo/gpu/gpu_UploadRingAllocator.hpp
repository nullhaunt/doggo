#pragma once

#include "doggo/doggo_Macro.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace doggo::gpu
{
  struct UploadRingSlice final
  {
      std::uintptr_t owner_cookie     = 0;
      std::uint64_t  virtual_position = 0;
      std::uint32_t  ring_generation  = 0;
      std::uint32_t  frame_index      = std::numeric_limits<std::uint32_t>::max();
      std::uint32_t  frame_generation = 0;
      std::uint32_t  offset           = 0;
      std::uint32_t  size             = 0;

      [[nodiscard]] bool isValid() const noexcept;

      constexpr bool operator==( const UploadRingSlice & ) const noexcept = default;
  };

  enum class UploadRingStatus : std::uint8_t
  {
    Success,
    NotInitialized,
    InvalidArgument,
    FrameAlreadyActive,
    NoActiveFrame,
    FrameIndexOutOfRange,
    FrameRetirementOutOfOrder,
    SizeOverflow,
    OutOfSpace,
  };

  struct UploadRingAllocationResult final
  {
      UploadRingStatus status = UploadRingStatus::NotInitialized;
      UploadRingSlice  slice  = {};
  };

  struct UploadRingSnapshot final
  {
      std::uint32_t capacity_bytes           = 0;
      std::uint32_t occupancy_bytes          = 0;
      std::uint32_t peak_occupancy_bytes     = 0;
      std::uint32_t available_bytes          = 0;
      std::uint32_t current_frame_bytes      = 0;
      std::uint32_t last_frame_bytes         = 0;
      std::uint32_t peak_frame_bytes         = 0;
      std::uint32_t configured_frame_count   = 0;
      std::uint32_t in_flight_frame_count    = 0;
      std::uint32_t allocation_failure_count = 0;
      std::uint64_t allocation_attempt_count = 0;
      std::uint64_t submitted_frame_count    = 0;
      std::uint64_t retired_frame_count      = 0;
      std::uint64_t total_submitted_bytes    = 0;
      std::uint64_t wrap_count               = 0;
      std::uint64_t stall_count              = 0;
      bool          has_active_frame         = false;
  };

  [[nodiscard]] const char * getUploadRingStatusName( UploadRingStatus status ) noexcept;

  // The owning render thread advances this ring. beginFrame confirms that the
  // selected frame context's previous completion fence has retired.
  class UploadRingAllocator final
  {
      DOGGO_DISALLOW_COPY( UploadRingAllocator );
      DOGGO_DISALLOW_MOVE( UploadRingAllocator );

    public:
      static constexpr std::size_t   MaximumFrameCount = 8;
      static constexpr std::uint32_t InvalidFrameIndex = std::numeric_limits<std::uint32_t>::max();

      UploadRingAllocator() noexcept = default;

      [[nodiscard]] bool initialize( std::uint32_t capacityBytes, std::uint32_t frameCount ) noexcept;
      void               finalize() noexcept;

      [[nodiscard]] UploadRingStatus beginFrame( std::uint32_t frameIndex, bool waitedForCompletion ) noexcept;
      [[nodiscard]] UploadRingAllocationResult allocate( std::uint32_t size, std::uint32_t alignment ) noexcept;
      [[nodiscard]] UploadRingStatus           endFrame() noexcept;
      [[nodiscard]] UploadRingStatus           abortFrame() noexcept;
      // The caller must confirm that every associated GPU fence has completed.
      [[nodiscard]] UploadRingStatus retireAllFrames() noexcept;

      [[nodiscard]] bool               isInitialized() const noexcept;
      [[nodiscard]] bool               owns( const UploadRingSlice & slice ) const noexcept;
      [[nodiscard]] UploadRingSnapshot snapshot() const noexcept;

    private:
      struct FrameState final
      {
          std::uint64_t start_position  = 0;
          std::uint64_t retire_position = 0;
          std::uint32_t generation      = 1;
          bool          is_active       = false;
          bool          is_in_flight    = false;
      };

      [[nodiscard]] static bool          alignUp( std::uint32_t value, std::uint32_t alignment,
                                                  std::uint32_t & alignedValue ) noexcept;
      [[nodiscard]] static std::uint32_t nextGeneration( std::uint32_t generation ) noexcept;
      static void                        addSaturated( std::uint64_t & value, std::uint64_t amount ) noexcept;
      static void                        incrementSaturated( std::uint32_t & value ) noexcept;
      static void                        incrementSaturated( std::uint64_t & value ) noexcept;

      void clearState() noexcept;

      std::array<FrameState, MaximumFrameCount> mFrames                 = {};
      std::uint64_t                             mHeadPosition           = 0;
      std::uint64_t                             mTailPosition           = 0;
      std::uint64_t                             mAllocationAttemptCount = 0;
      std::uint64_t                             mSubmittedFrameCount    = 0;
      std::uint64_t                             mRetiredFrameCount      = 0;
      std::uint64_t                             mTotalSubmittedBytes    = 0;
      std::uint64_t                             mWrapCount              = 0;
      std::uint64_t                             mStallCount             = 0;
      std::uint64_t                             mCurrentFrameWrapCount  = 0;
      std::uint32_t                             mCapacityBytes          = 0;
      std::uint32_t                             mPeakOccupancyBytes     = 0;
      std::uint32_t                             mCurrentFrameBytes      = 0;
      std::uint32_t                             mLastFrameBytes         = 0;
      std::uint32_t                             mPeakFrameBytes         = 0;
      std::uint32_t                             mFrameCount             = 0;
      std::uint32_t                             mActiveFrameIndex       = InvalidFrameIndex;
      std::uint32_t                             mAllocationFailureCount = 0;
      std::uint32_t                             mRingGeneration         = 0;
      bool                                      mIsInitialized          = false;
  };
}  // namespace doggo::gpu
