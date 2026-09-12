#pragma once

#include "doggo/doggo_Macro.hpp"
#include "doggo/gpu/gpu_ArenaAllocator.hpp"

#include <deko3d.hpp>

#include <span>

namespace doggo::gpu::deko
{
  class MemoryArena;

  enum class GraphicsProgramStatus : std::uint8_t
  {
    Success,
    InvalidArgument,
    CodeArenaNotInitialized,
    ShaderCodeSizeOverflow,
    CodeArenaAllocationFailed,
    CodeMemoryMappingFailed,
    InvalidVertexShader,
    InvalidFragmentShader,
    UnexpectedVertexStage,
    UnexpectedFragmentStage,
  };

  [[nodiscard]] const char * getGraphicsProgramStatusName( GraphicsProgramStatus status ) noexcept;

  class GraphicsProgram final
  {
      DOGGO_DISALLOW_COPY( GraphicsProgram );
      DOGGO_DISALLOW_MOVE( GraphicsProgram );

    public:
      GraphicsProgram() noexcept = default;
      ~GraphicsProgram();

      // The code arena must outlive the program. Its release policy controls whether finalize can reuse the range.
      [[nodiscard]] GraphicsProgramStatus initialize( MemoryArena &                 codeArena,
                                                      std::span<const std::uint8_t> vertexBinary,
                                                      std::span<const std::uint8_t> fragmentBinary ) noexcept;
      void                                finalize() noexcept;

      void bind( dk::CmdBuf commandBuffer ) const noexcept;

      [[nodiscard]] bool          isInitialized() const noexcept;
      [[nodiscard]] std::uint32_t codeMemorySize() const noexcept;

    private:
      MemoryArena *   mCodeArena = nullptr;
      ArenaAllocation mCodeAllocation;
      dk::Shader      mVertexShader;
      dk::Shader      mFragmentShader;
      std::uint32_t   mCodeMemorySize = 0;
  };
}  // namespace doggo::gpu::deko
