#pragma once

#include "doggo/doggo_Macro.hpp"

#include <deko3d.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace doggo::gpu::deko
{
  enum class GraphicsProgramStatus : std::uint8_t
  {
    Success,
    InvalidArgument,
    ShaderCodeSizeOverflow,
    CodeMemoryCreationFailed,
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

      [[nodiscard]] GraphicsProgramStatus initialize( dk::Device                    device,
                                                      std::span<const std::uint8_t> vertexBinary,
                                                      std::span<const std::uint8_t> fragmentBinary ) noexcept;
      void                                finalize() noexcept;

      void bind( dk::CmdBuf commandBuffer ) const noexcept;

      [[nodiscard]] bool          isInitialized() const noexcept;
      [[nodiscard]] std::uint32_t codeMemorySize() const noexcept;

    private:
      dk::UniqueMemBlock mCodeMemory;
      dk::Shader         mVertexShader;
      dk::Shader         mFragmentShader;
      std::uint32_t      mCodeMemorySize = 0;
  };
}  // namespace doggo::gpu::deko
