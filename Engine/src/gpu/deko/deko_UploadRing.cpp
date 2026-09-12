#include "doggo/gpu/deko/deko_UploadRing.hpp"

#include <limits>

namespace
{
  [[nodiscard]] bool
  alignUp( const std::uint32_t value, const std::uint32_t alignment, std::uint32_t & alignedValue ) noexcept
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
}  // namespace

namespace doggo::gpu::deko
{
  const char * getUploadRingInitializationStatusName( const UploadRingInitializationStatus status ) noexcept
  {
    switch ( status )
    {
      case UploadRingInitializationStatus::Success:
        return "Success";

      case UploadRingInitializationStatus::InvalidArgument:
        return "Invalid Argument";

      case UploadRingInitializationStatus::SizeOverflow:
        return "Size Overflow";

      case UploadRingInitializationStatus::MemoryCreationFailed:
        return "Memory Creation Failed";

      case UploadRingInitializationStatus::CpuMappingUnavailable:
        return "CPU Mapping Unavailable";

      case UploadRingInitializationStatus::AllocatorInitializationFailed:
        return "Allocator Initialization Failed";
    }

    return "Unknown";
  }

  UploadRing::~UploadRing()
  {
    finalize();
  }

  UploadRingInitializationStatus UploadRing::initialize( const dk::Device         device,
                                                         const UploadRingConfig & config ) noexcept
  {
    if ( isInitialized() )
    {
      return UploadRingInitializationStatus::Success;
    }

    finalize();

    if ( !device ||
         config.capacity_bytes == 0 ||
         config.frame_count == 0 ||
         config.frame_count > UploadRingAllocator::MaximumFrameCount )
    {
      return UploadRingInitializationStatus::InvalidArgument;
    }

    std::uint32_t memoryBlockSize = 0;
    if ( !alignUp( config.capacity_bytes, DK_MEMBLOCK_ALIGNMENT, memoryBlockSize ) )
    {
      return UploadRingInitializationStatus::SizeOverflow;
    }

    mMemory = dk::MemBlockMaker{ device, memoryBlockSize }
                  .setFlags( DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached )
                  .create();
    if ( !mMemory )
    {
      return UploadRingInitializationStatus::MemoryCreationFailed;
    }

    mCpuAddress = static_cast<std::byte *>( mMemory.getCpuAddr() );
    if ( !mCpuAddress )
    {
      finalize();
      return UploadRingInitializationStatus::CpuMappingUnavailable;
    }

    if ( !mAllocator.initialize( memoryBlockSize, config.frame_count ) )
    {
      finalize();
      return UploadRingInitializationStatus::AllocatorInitializationFailed;
    }

    mGpuAddress           = mMemory.getGpuAddr();
    mMemoryBlockSizeBytes = memoryBlockSize;
    return UploadRingInitializationStatus::Success;
  }

  void UploadRing::finalize() noexcept
  {
    mAllocator.finalize();
    mMemory               = nullptr;
    mCpuAddress           = nullptr;
    mGpuAddress           = 0;
    mMemoryBlockSizeBytes = 0;
  }

  UploadRingStatus UploadRing::beginFrame( const std::uint32_t frameIndex, const bool waitedForCompletion ) noexcept
  {
    return mAllocator.beginFrame( frameIndex, waitedForCompletion );
  }

  UploadRingAllocationResult UploadRing::allocate( const std::uint32_t size, const std::uint32_t alignment ) noexcept
  {
    return mAllocator.allocate( size, alignment );
  }

  UploadRingStatus UploadRing::endFrame() noexcept
  {
    return mAllocator.endFrame();
  }

  UploadRingStatus UploadRing::abortFrame() noexcept
  {
    return mAllocator.abortFrame();
  }

  UploadRingStatus UploadRing::retireAllFrames() noexcept
  {
    return mAllocator.retireAllFrames();
  }

  bool UploadRing::isInitialized() const noexcept
  {
    return static_cast<bool>( mMemory ) && mCpuAddress && mAllocator.isInitialized();
  }

  bool UploadRing::owns( const UploadRingSlice & slice ) const noexcept
  {
    return mAllocator.owns( slice );
  }

  std::span<std::byte> UploadRing::cpuSpan( const UploadRingSlice & slice ) noexcept
  {
    if ( !mCpuAddress || !owns( slice ) )
    {
      return {};
    }

    return { mCpuAddress + slice.offset, slice.size };
  }

  DkGpuAddr UploadRing::gpuAddress( const UploadRingSlice & slice ) const noexcept
  {
    if ( !owns( slice ) )
    {
      return 0;
    }

    return mGpuAddress + slice.offset;
  }

  DekoUploadRingSnapshot UploadRing::snapshot() const noexcept
  {
    return {
        .allocations             = mAllocator.snapshot(),
        .memory_block_size_bytes = mMemoryBlockSizeBytes,
        .is_cpu_mapped           = mCpuAddress != nullptr,
    };
  }
}  // namespace doggo::gpu::deko
