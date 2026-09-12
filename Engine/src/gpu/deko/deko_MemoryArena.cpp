#include "doggo/gpu/deko/deko_MemoryArena.hpp"

#include <limits>

namespace
{
  [[nodiscard]] bool
  alignUp( const std::uint32_t value, const std::uint32_t alignment, std::uint32_t & alignedValue ) noexcept
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
}  // namespace

namespace doggo::gpu::deko
{
  const char * getMemoryArenaStatusName( const MemoryArenaStatus status ) noexcept
  {
    switch ( status )
    {
      case MemoryArenaStatus::Success:
        return "Success";
      case MemoryArenaStatus::InvalidArgument:
        return "Invalid Argument";
      case MemoryArenaStatus::SizeOverflow:
        return "Size Overflow";
      case MemoryArenaStatus::MemoryCreationFailed:
        return "Memory Creation Failed";
      case MemoryArenaStatus::CpuMappingUnavailable:
        return "CPU Mapping Unavailable";
      case MemoryArenaStatus::AllocatorInitializationFailed:
        return "Allocator Initialization Failed";
    }

    return "Unknown";
  }

  MemoryArena::~MemoryArena()
  {
    finalize();
  }

  MemoryArenaStatus MemoryArena::initialize( const dk::Device device, const MemoryArenaConfig & config ) noexcept
  {
    if ( isInitialized() )
    {
      return MemoryArenaStatus::Success;
    }

    finalize();

    if ( !device ||
         config.memory_block_size_bytes == 0 ||
         ( config.flags & DkMemBlockFlags_GpuAccessMask ) == 0 ||
         config.reserved_tail_bytes >= config.memory_block_size_bytes )
    {
      return MemoryArenaStatus::InvalidArgument;
    }

    std::uint32_t memoryBlockSize = 0;
    if ( !alignUp( config.memory_block_size_bytes, DK_MEMBLOCK_ALIGNMENT, memoryBlockSize ) ||
         memoryBlockSize <= config.reserved_tail_bytes )
    {
      return MemoryArenaStatus::SizeOverflow;
    }

    mMemory = dk::MemBlockMaker{ device, memoryBlockSize }.setFlags( config.flags ).create();
    if ( !mMemory )
    {
      return MemoryArenaStatus::MemoryCreationFailed;
    }

    mCpuAddress = static_cast<std::byte *>( mMemory.getCpuAddr() );
    if ( config.require_cpu_mapping && !mCpuAddress )
    {
      finalize();
      return MemoryArenaStatus::CpuMappingUnavailable;
    }

    if ( !mAllocator.initialize( memoryBlockSize - config.reserved_tail_bytes ) )
    {
      finalize();
      return MemoryArenaStatus::AllocatorInitializationFailed;
    }

    mGpuAddress           = mMemory.getGpuAddr();
    mMemoryBlockSizeBytes = memoryBlockSize;
    mReservedTailBytes    = config.reserved_tail_bytes;
    mFlags                = config.flags;
    mReleasePolicy        = config.release_policy;
    return MemoryArenaStatus::Success;
  }

  void MemoryArena::finalize() noexcept
  {
    mAllocator.finalize();
    mMemory               = nullptr;
    mCpuAddress           = nullptr;
    mGpuAddress           = 0;
    mMemoryBlockSizeBytes = 0;
    mReservedTailBytes    = 0;
    mFlags                = 0;
    mReleasePolicy        = MemoryArenaReleasePolicy::Reusable;
  }

  ArenaAllocationResult MemoryArena::allocate( const std::uint32_t size, const std::uint32_t alignment ) noexcept
  {
    return mAllocator.allocate( size, alignment );
  }

  ArenaReleaseStatus MemoryArena::release( const ArenaAllocation & allocation ) noexcept
  {
    if ( !isInitialized() )
    {
      return ArenaReleaseStatus::NotInitialized;
    }

    if ( !mAllocator.owns( allocation ) )
    {
      return mAllocator.release( allocation );
    }

    if ( mReleasePolicy == MemoryArenaReleasePolicy::RetainedUntilFinalize )
    {
      return ArenaReleaseStatus::ReleaseDisabled;
    }

    return mAllocator.release( allocation );
  }

  bool MemoryArena::isInitialized() const noexcept
  {
    return static_cast<bool>( mMemory ) && mAllocator.isInitialized();
  }

  bool MemoryArena::owns( const ArenaAllocation & allocation ) const noexcept
  {
    return mAllocator.owns( allocation );
  }

  std::span<std::byte> MemoryArena::cpuSpan( const ArenaAllocation & allocation ) noexcept
  {
    if ( !mCpuAddress || !owns( allocation ) )
    {
      return {};
    }

    return { mCpuAddress + allocation.offset, allocation.size };
  }

  DkGpuAddr MemoryArena::gpuAddress( const ArenaAllocation & allocation ) const noexcept
  {
    if ( !owns( allocation ) )
    {
      return 0;
    }

    return mGpuAddress + allocation.offset;
  }

  dk::MemBlock MemoryArena::memoryBlock() const noexcept
  {
    return mMemory;
  }

  MemoryArenaSnapshot MemoryArena::snapshot() const noexcept
  {
    return {
        .allocations             = mAllocator.snapshot(),
        .memory_block_size_bytes = mMemoryBlockSizeBytes,
        .reserved_tail_bytes     = mReservedTailBytes,
        .flags                   = mFlags,
        .release_policy          = mReleasePolicy,
        .is_cpu_mapped           = mCpuAddress != nullptr,
    };
  }
}  // namespace doggo::gpu::deko
