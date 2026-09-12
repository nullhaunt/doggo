#include <doggo/gpu/gpu_ArenaAllocator.hpp>
#include <doggo/gpu/gpu_UploadRingAllocator.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
  using doggo::gpu::ArenaAllocation;
  using doggo::gpu::ArenaAllocationStatus;
  using doggo::gpu::ArenaAllocator;
  using doggo::gpu::ArenaReleaseStatus;
  using doggo::gpu::UploadRingAllocator;
  using doggo::gpu::UploadRingSlice;
  using doggo::gpu::UploadRingStatus;

  void require( const bool condition, const std::string_view message )
  {
    if ( !condition )
    {
      throw std::runtime_error{ std::string{ message } };
    }
  }

  [[nodiscard]] ArenaAllocation allocate( ArenaAllocator &       allocator,
                                          const std::uint32_t    size,
                                          const std::uint32_t    alignment,
                                          const std::string_view message )
  {
    const doggo::gpu::ArenaAllocationResult result = allocator.allocate( size, alignment );
    require( result.status == ArenaAllocationStatus::Success, message );
    require( result.allocation.isValid(), "successful allocation returned an invalid handle" );
    require( allocator.owns( result.allocation ), "allocator does not own its successful allocation" );
    return result.allocation;
  }

  [[nodiscard]] UploadRingSlice allocateUpload( UploadRingAllocator &  allocator,
                                                const std::uint32_t    size,
                                                const std::uint32_t    alignment,
                                                const std::string_view message )
  {
    const doggo::gpu::UploadRingAllocationResult result = allocator.allocate( size, alignment );
    require( result.status == UploadRingStatus::Success, message );
    require( result.slice.isValid(), "successful upload allocation returned an invalid slice" );
    require( allocator.owns( result.slice ), "upload ring does not own its successful allocation" );
    return result.slice;
  }

  void testInitializationAndAlignedPlacement()
  {
    ArenaAllocator allocator;
    require( !allocator.initialize( 0 ), "zero-capacity arena initialized" );
    require( allocator.initialize( 1024 ), "arena initialization failed" );
    require( !allocator.initialize( 1024 ), "initialized arena accepted reinitialization" );

    const ArenaAllocation first  = allocate( allocator, 100, 64, "first aligned allocation failed" );
    const ArenaAllocation second = allocate( allocator, 100, 256, "second aligned allocation failed" );
    require( first.offset == 0, "first allocation did not begin at zero" );
    require( second.offset == 256, "second allocation did not honor alignment" );

    const doggo::gpu::ArenaSnapshot snapshot = allocator.snapshot();
    require( snapshot.capacity_bytes == 1024, "snapshot capacity is incorrect" );
    require( snapshot.used_bytes == 200, "snapshot used bytes are incorrect" );
    require( snapshot.free_bytes == 824, "snapshot free bytes are incorrect" );
    require( snapshot.peak_used_bytes == 200, "snapshot peak bytes are incorrect" );
    require( snapshot.allocation_capacity == ArenaAllocator::MaximumAllocationCount,
             "snapshot allocation capacity is incorrect" );
    require( snapshot.live_allocation_count == 2, "snapshot live count is incorrect" );
    require( snapshot.free_range_count == 2, "alignment padding was not retained as a free range" );
    require( snapshot.largest_free_range_bytes == 668, "largest free range is incorrect" );
    require( snapshot.fragmentation_per_mille == 189, "fragmentation metric is incorrect" );
  }

  void testDeterministicBestFitTieBreak()
  {
    ArenaAllocator allocator;
    require( allocator.initialize( 512 ), "arena initialization failed" );

    const ArenaAllocation first  = allocate( allocator, 64, 1, "first allocation failed" );
    const ArenaAllocation lower  = allocate( allocator, 128, 1, "lower hole allocation failed" );
    const ArenaAllocation middle = allocate( allocator, 64, 1, "middle allocation failed" );
    const ArenaAllocation upper  = allocate( allocator, 128, 1, "upper hole allocation failed" );
    const ArenaAllocation guard  = allocate( allocator, 128, 1, "guard allocation failed" );
    ( void )first;
    ( void )middle;
    ( void )guard;

    require( allocator.release( lower ) == ArenaReleaseStatus::Success, "lower hole release failed" );
    require( allocator.release( upper ) == ArenaReleaseStatus::Success, "upper hole release failed" );

    const ArenaAllocation selected = allocate( allocator, 64, 1, "best-fit allocation failed" );
    require( selected.offset == lower.offset, "equal best-fit ranges did not select the lower offset" );
  }

  void testReleaseCoalescingAndStaleHandles()
  {
    ArenaAllocator allocator;
    require( allocator.initialize( 1024 ), "arena initialization failed" );

    const ArenaAllocation first  = allocate( allocator, 100, 1, "first allocation failed" );
    const ArenaAllocation second = allocate( allocator, 100, 1, "second allocation failed" );
    const ArenaAllocation third  = allocate( allocator, 100, 1, "third allocation failed" );

    require( allocator.release( second ) == ArenaReleaseStatus::Success, "middle release failed" );
    require( allocator.release( first ) == ArenaReleaseStatus::Success, "leading release failed" );
    require( allocator.release( third ) == ArenaReleaseStatus::Success, "trailing release failed" );

    const doggo::gpu::ArenaSnapshot snapshot = allocator.snapshot();
    require( snapshot.used_bytes == 0, "released arena still reports used bytes" );
    require( snapshot.free_range_count == 1, "adjacent free ranges did not coalesce" );
    require( snapshot.largest_free_range_bytes == 1024, "coalesced range does not span the arena" );
    require( snapshot.fragmentation_per_mille == 0, "empty arena reports fragmentation" );

    require( allocator.release( second ) == ArenaReleaseStatus::StaleAllocation,
             "double release did not report a stale allocation" );
    require( allocator.snapshot().invalid_release_count == 1, "invalid release counter was not updated" );
  }

  void testForeignAndReinitializedHandles()
  {
    ArenaAllocator firstAllocator;
    ArenaAllocator secondAllocator;
    require( firstAllocator.initialize( 256 ), "first arena initialization failed" );
    require( secondAllocator.initialize( 256 ), "second arena initialization failed" );

    const ArenaAllocation allocation = allocate( firstAllocator, 32, 8, "allocation failed" );
    require( secondAllocator.release( allocation ) == ArenaReleaseStatus::ForeignAllocation,
             "foreign allocation was not rejected" );

    firstAllocator.finalize();
    require( firstAllocator.initialize( 256 ), "arena reinitialization failed" );
    require( firstAllocator.release( allocation ) == ArenaReleaseStatus::StaleAllocation,
             "pre-finalize allocation remained valid after reinitialization" );
  }

  void testFailureDiagnosticsAndOverflow()
  {
    ArenaAllocator allocator;
    require( allocator.allocate( 1, 1 ).status == ArenaAllocationStatus::NotInitialized,
             "uninitialized allocation returned the wrong status" );
    require( allocator.initialize( 64 ), "arena initialization failed" );
    require( allocator.allocate( 0, 1 ).status == ArenaAllocationStatus::InvalidArgument,
             "zero-size allocation returned the wrong status" );
    require( allocator.allocate( 1, 0 ).status == ArenaAllocationStatus::InvalidArgument,
             "zero-alignment allocation returned the wrong status" );
    require( allocator.allocate( 65, 1 ).status == ArenaAllocationStatus::OutOfMemory,
             "oversized allocation returned the wrong status" );

    const doggo::gpu::ArenaSnapshot snapshot = allocator.snapshot();
    require( snapshot.allocation_attempt_count == 3, "allocation attempts were not counted" );
    require( snapshot.allocation_failure_count == 3, "allocation failures were not counted" );

    ArenaAllocator overflowAllocator;
    require( overflowAllocator.initialize( std::numeric_limits<std::uint32_t>::max() ),
             "maximum-size arena initialization failed" );
    ( void )allocate( overflowAllocator, 1, 1, "overflow setup allocation failed" );
    require( overflowAllocator.allocate( 1, std::numeric_limits<std::uint32_t>::max() ).status ==
                 ArenaAllocationStatus::SizeOverflow,
             "alignment overflow returned the wrong status" );
  }

  void testAllocationLimit()
  {
    ArenaAllocator allocator;
    require( allocator.initialize( static_cast<std::uint32_t>( ArenaAllocator::MaximumAllocationCount + 1 ) ),
             "arena initialization failed" );

    for ( std::size_t index = 0; index < ArenaAllocator::MaximumAllocationCount; ++index )
    {
      ( void )allocate( allocator, 1, 1, "allocation table filled prematurely" );
    }

    require( allocator.allocate( 1, 1 ).status == ArenaAllocationStatus::AllocationLimitReached,
             "full allocation table returned the wrong status" );
    require( allocator.snapshot().allocation_failure_count == 1, "allocation limit failure was not counted" );
  }

  void testUploadRingLifecycleAndWrap()
  {
    UploadRingAllocator allocator;
    require( !allocator.initialize( 0, 3 ), "zero-capacity upload ring initialized" );
    require( !allocator.initialize( 128, 0 ), "zero-frame upload ring initialized" );
    require( allocator.initialize( 100, 3 ), "upload ring initialization failed" );

    require( allocator.beginFrame( 0, false ) == UploadRingStatus::Success, "frame zero did not begin" );
    const UploadRingSlice first = allocateUpload( allocator, 30, 8, "first upload allocation failed" );
    require( first.offset == 0, "first upload allocation did not begin at zero" );
    require( allocator.endFrame() == UploadRingStatus::Success, "frame zero did not end" );

    require( allocator.beginFrame( 1, false ) == UploadRingStatus::Success, "frame one did not begin" );
    const UploadRingSlice second = allocateUpload( allocator, 20, 16, "second upload allocation failed" );
    require( second.offset == 32, "second upload allocation did not honor alignment" );
    require( allocator.endFrame() == UploadRingStatus::Success, "frame one did not end" );

    require( allocator.beginFrame( 2, false ) == UploadRingStatus::Success, "frame two did not begin" );
    const UploadRingSlice third = allocateUpload( allocator, 30, 64, "third upload allocation failed" );
    require( third.offset == 64, "third upload allocation did not honor alignment" );
    require( allocator.endFrame() == UploadRingStatus::Success, "frame two did not end" );

    require( allocator.beginFrame( 0, true ) == UploadRingStatus::Success, "frame zero did not retire" );
    require( !allocator.owns( first ), "retired upload slice remained valid" );
    const UploadRingSlice wrapped = allocateUpload( allocator, 16, 8, "wrapped upload allocation failed" );
    require( wrapped.offset == 0, "upload ring did not wrap to zero" );

    const doggo::gpu::UploadRingSnapshot snapshot = allocator.snapshot();
    require( snapshot.capacity_bytes == 100, "upload snapshot capacity is incorrect" );
    require( snapshot.occupancy_bytes == 86, "upload snapshot occupancy is incorrect" );
    require( snapshot.available_bytes == 14, "upload snapshot availability is incorrect" );
    require( snapshot.current_frame_bytes == 16, "current-frame upload bytes are incorrect" );
    require( snapshot.last_frame_bytes == 30, "last-frame upload bytes are incorrect" );
    require( snapshot.peak_frame_bytes == 30, "peak frame upload bytes are incorrect" );
    require( snapshot.in_flight_frame_count == 2, "in-flight upload frame count is incorrect" );
    require( snapshot.submitted_frame_count == 3, "submitted upload frame count is incorrect" );
    require( snapshot.retired_frame_count == 1, "retired upload frame count is incorrect" );
    require( snapshot.total_submitted_bytes == 80, "total submitted upload bytes are incorrect" );
    require( snapshot.wrap_count == 1, "upload wrap count is incorrect" );
    require( snapshot.stall_count == 1, "upload stall count is incorrect" );
  }

  void testUploadRingFailuresAndRetirementOrder()
  {
    UploadRingAllocator allocator;
    require( allocator.allocate( 1, 1 ).status == UploadRingStatus::NotInitialized,
             "uninitialized upload allocation returned the wrong status" );
    require( allocator.initialize( 64, 3 ), "upload ring initialization failed" );
    require( allocator.beginFrame( 3, false ) == UploadRingStatus::FrameIndexOutOfRange,
             "out-of-range upload frame was accepted" );

    require( allocator.beginFrame( 0, false ) == UploadRingStatus::Success, "frame zero did not begin" );
    ( void )allocateUpload( allocator, 32, 1, "frame zero upload failed" );
    require( allocator.beginFrame( 1, false ) == UploadRingStatus::FrameAlreadyActive,
             "upload ring accepted two active frames" );
    require( allocator.endFrame() == UploadRingStatus::Success, "frame zero did not end" );

    require( allocator.beginFrame( 1, false ) == UploadRingStatus::Success, "frame one did not begin" );
    ( void )allocateUpload( allocator, 16, 1, "frame one upload failed" );
    require( allocator.allocate( 17, 1 ).status == UploadRingStatus::OutOfSpace,
             "exhausted upload ring returned the wrong status" );
    require( allocator.endFrame() == UploadRingStatus::Success, "frame one did not end" );

    require( allocator.beginFrame( 1, false ) == UploadRingStatus::FrameRetirementOutOfOrder,
             "out-of-order upload retirement was accepted" );
    require( allocator.beginFrame( 0, false ) == UploadRingStatus::Success, "oldest upload frame did not retire" );
    require( allocator.abortFrame() == UploadRingStatus::Success, "empty upload frame did not abort" );

    const doggo::gpu::UploadRingSnapshot snapshot = allocator.snapshot();
    require( snapshot.allocation_attempt_count == 3, "upload allocation attempts were not counted" );
    require( snapshot.allocation_failure_count == 1, "upload allocation failures were not counted" );
  }

  void testUploadRingAbortAndMassRetirement()
  {
    UploadRingAllocator allocator;
    require( allocator.initialize( 64, 2 ), "upload ring initialization failed" );
    require( allocator.beginFrame( 0, false ) == UploadRingStatus::Success, "frame zero did not begin" );
    const UploadRingSlice aborted = allocateUpload( allocator, 48, 1, "aborted upload allocation failed" );
    require( allocator.abortFrame() == UploadRingStatus::Success, "upload frame did not abort" );
    require( !allocator.owns( aborted ), "aborted upload slice remained valid" );
    require( allocator.snapshot().occupancy_bytes == 0, "aborted upload bytes remained occupied" );

    require( allocator.beginFrame( 0, false ) == UploadRingStatus::Success, "frame zero did not restart" );
    ( void )allocateUpload( allocator, 32, 1, "frame zero upload failed" );
    require( allocator.endFrame() == UploadRingStatus::Success, "frame zero did not end" );
    require( allocator.beginFrame( 1, false ) == UploadRingStatus::Success, "frame one did not begin" );
    ( void )allocateUpload( allocator, 16, 1, "frame one upload failed" );
    require( allocator.endFrame() == UploadRingStatus::Success, "frame one did not end" );
    require( allocator.retireAllFrames() == UploadRingStatus::Success, "mass retirement failed" );

    const doggo::gpu::UploadRingSnapshot snapshot = allocator.snapshot();
    require( snapshot.occupancy_bytes == 0, "mass retirement did not empty the upload ring" );
    require( snapshot.in_flight_frame_count == 0, "mass retirement left frames in flight" );
    require( snapshot.retired_frame_count == 2, "mass retirement count is incorrect" );
  }

  void testUploadRingLongRun()
  {
    UploadRingAllocator allocator;
    require( allocator.initialize( 256, 3 ), "upload ring initialization failed" );

    std::uint64_t expectedSubmittedBytes = 0;
    for ( std::uint32_t frame = 0; frame < 1'000; ++frame )
    {
      require( allocator.beginFrame( frame % 3, false ) == UploadRingStatus::Success,
               "long-run upload frame did not begin" );
      const std::uint32_t   size      = 17 + frame % 5;
      const std::uint32_t   alignment = 1u << ( frame % 4 );
      const UploadRingSlice slice     = allocateUpload( allocator, size, alignment, "long-run upload failed" );
      require( slice.offset % alignment == 0, "long-run upload alignment is incorrect" );
      require( allocator.snapshot().occupancy_bytes <= allocator.snapshot().capacity_bytes,
               "long-run upload occupancy exceeded capacity" );
      require( allocator.endFrame() == UploadRingStatus::Success, "long-run upload frame did not end" );
      expectedSubmittedBytes += size;
    }

    require( allocator.retireAllFrames() == UploadRingStatus::Success, "long-run retirement failed" );
    const doggo::gpu::UploadRingSnapshot snapshot = allocator.snapshot();
    require( snapshot.occupancy_bytes == 0, "long-run retirement did not empty the upload ring" );
    require( snapshot.submitted_frame_count == 1'000, "long-run submitted frame count is incorrect" );
    require( snapshot.total_submitted_bytes == expectedSubmittedBytes, "long-run submitted byte count is incorrect" );
    require( snapshot.wrap_count != 0, "long-run upload ring never wrapped" );
  }
}  // namespace

int main()
{
  try
  {
    testInitializationAndAlignedPlacement();
    testDeterministicBestFitTieBreak();
    testReleaseCoalescingAndStaleHandles();
    testForeignAndReinitializedHandles();
    testFailureDiagnosticsAndOverflow();
    testAllocationLimit();
    testUploadRingLifecycleAndWrap();
    testUploadRingFailuresAndRetirementOrder();
    testUploadRingAbortAndMassRetirement();
    testUploadRingLongRun();
    std::cout << "doggo-host-tests: all tests passed\n";
    return 0;
  }
  catch ( const std::exception & error )
  {
    std::cerr << "doggo-host-tests: " << error.what() << '\n';
    return 1;
  }
}
