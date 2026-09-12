#pragma once

#include "doggo/doggo_Macro.hpp"
#include "doggo/gpu/gpu_UploadRingAllocator.hpp"

#include <deko3d.hpp>

#include <cstddef>
#include <span>

namespace doggo::gpu::deko
{
  enum class UploadRingInitializationStatus : std::uint8_t
  {
    Success,
    InvalidArgument,
    SizeOverflow,
    MemoryCreationFailed,
    CpuMappingUnavailable,
    AllocatorInitializationFailed,
  };

  struct UploadRingConfig final
  {
      std::uint32_t capacity_bytes = 0;
      std::uint32_t frame_count    = 0;
  };

  struct DekoUploadRingSnapshot final
  {
      UploadRingSnapshot allocations             = {};
      std::uint32_t      memory_block_size_bytes = 0;
      bool               is_cpu_mapped           = false;
  };

  [[nodiscard]] const char * getUploadRingInitializationStatusName( UploadRingInitializationStatus status ) noexcept;

  class UploadRing final
  {
      DOGGO_DISALLOW_COPY( UploadRing );
      DOGGO_DISALLOW_MOVE( UploadRing );

    public:
      UploadRing() noexcept = default;
      ~UploadRing();

      [[nodiscard]] UploadRingInitializationStatus initialize( dk::Device               device,
                                                               const UploadRingConfig & config ) noexcept;
      void                                         finalize() noexcept;

      [[nodiscard]] UploadRingStatus beginFrame( std::uint32_t frameIndex, bool waitedForCompletion ) noexcept;
      [[nodiscard]] UploadRingAllocationResult allocate( std::uint32_t size, std::uint32_t alignment ) noexcept;
      [[nodiscard]] UploadRingStatus           endFrame() noexcept;
      [[nodiscard]] UploadRingStatus           abortFrame() noexcept;
      [[nodiscard]] UploadRingStatus           retireAllFrames() noexcept;

      [[nodiscard]] bool                   isInitialized() const noexcept;
      [[nodiscard]] bool                   owns( const UploadRingSlice & slice ) const noexcept;
      [[nodiscard]] std::span<std::byte>   cpuSpan( const UploadRingSlice & slice ) noexcept;
      [[nodiscard]] DkGpuAddr              gpuAddress( const UploadRingSlice & slice ) const noexcept;
      [[nodiscard]] DekoUploadRingSnapshot snapshot() const noexcept;

    private:
      UploadRingAllocator mAllocator;
      dk::UniqueMemBlock  mMemory;
      std::byte *         mCpuAddress           = nullptr;
      DkGpuAddr           mGpuAddress           = 0;
      std::uint32_t       mMemoryBlockSizeBytes = 0;
  };
}  // namespace doggo::gpu::deko
