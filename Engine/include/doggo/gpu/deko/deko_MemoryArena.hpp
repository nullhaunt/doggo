#pragma once

#include "doggo/doggo_Macro.hpp"
#include "doggo/gpu/gpu_ArenaAllocator.hpp"

#include <deko3d.hpp>

#include <cstddef>
#include <span>

namespace doggo::gpu::deko
{
  enum class MemoryArenaReleasePolicy : std::uint8_t
  {
    Reusable,
    RetainedUntilFinalize,
  };

  enum class MemoryArenaStatus : std::uint8_t
  {
    Success,
    InvalidArgument,
    SizeOverflow,
    MemoryCreationFailed,
    CpuMappingUnavailable,
    AllocatorInitializationFailed,
  };

  struct MemoryArenaConfig final
  {
      std::uint32_t            memory_block_size_bytes = 0;
      std::uint32_t            reserved_tail_bytes     = 0;
      std::uint32_t            flags                   = 0;
      MemoryArenaReleasePolicy release_policy          = MemoryArenaReleasePolicy::Reusable;
      bool                     require_cpu_mapping     = false;
  };

  struct MemoryArenaSnapshot final
  {
      ArenaSnapshot            allocations             = {};
      std::uint32_t            memory_block_size_bytes = 0;
      std::uint32_t            reserved_tail_bytes     = 0;
      std::uint32_t            flags                   = 0;
      MemoryArenaReleasePolicy release_policy          = MemoryArenaReleasePolicy::Reusable;
      bool                     is_cpu_mapped           = false;
  };

  [[nodiscard]] const char * getMemoryArenaStatusName( MemoryArenaStatus status ) noexcept;

  class MemoryArena final
  {
      DOGGO_DISALLOW_COPY( MemoryArena );
      DOGGO_DISALLOW_MOVE( MemoryArena );

    public:
      MemoryArena() noexcept = default;
      ~MemoryArena();

      [[nodiscard]] MemoryArenaStatus initialize( dk::Device device, const MemoryArenaConfig & config ) noexcept;
      void                            finalize() noexcept;

      [[nodiscard]] ArenaAllocationResult allocate( std::uint32_t size, std::uint32_t alignment ) noexcept;
      [[nodiscard]] ArenaReleaseStatus    release( const ArenaAllocation & allocation ) noexcept;

      [[nodiscard]] bool                 isInitialized() const noexcept;
      [[nodiscard]] bool                 owns( const ArenaAllocation & allocation ) const noexcept;
      [[nodiscard]] std::span<std::byte> cpuSpan( const ArenaAllocation & allocation ) noexcept;
      [[nodiscard]] DkGpuAddr            gpuAddress( const ArenaAllocation & allocation ) const noexcept;
      [[nodiscard]] dk::MemBlock         memoryBlock() const noexcept;
      [[nodiscard]] MemoryArenaSnapshot  snapshot() const noexcept;

    private:
      ArenaAllocator           mAllocator;
      dk::UniqueMemBlock       mMemory;
      std::byte *              mCpuAddress           = nullptr;
      DkGpuAddr                mGpuAddress           = 0;
      std::uint32_t            mMemoryBlockSizeBytes = 0;
      std::uint32_t            mReservedTailBytes    = 0;
      std::uint32_t            mFlags                = 0;
      MemoryArenaReleasePolicy mReleasePolicy        = MemoryArenaReleasePolicy::Reusable;
  };
}  // namespace doggo::gpu::deko
