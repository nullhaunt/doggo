#include "doggo/gpu/deko/deko_GraphicsProgram.hpp"

#include <cstring>
#include <limits>

namespace
{
  [[nodiscard]] bool
  alignUp( const std::uint64_t value, const std::uint32_t alignment, std::uint64_t & alignedValue ) noexcept
  {
    if ( alignment == 0 )
    {
      return false;
    }

    const std::uint64_t remainder = value % alignment;
    if ( remainder == 0 )
    {
      alignedValue = value;
      return true;
    }

    const std::uint64_t padding = alignment - remainder;
    if ( value > std::numeric_limits<std::uint64_t>::max() - padding )
    {
      return false;
    }

    alignedValue = value + padding;
    return true;
  }

  [[nodiscard]] bool
  addWithoutOverflow( const std::uint64_t left, const std::uint64_t right, std::uint64_t & sum ) noexcept
  {
    if ( left > std::numeric_limits<std::uint64_t>::max() - right )
    {
      return false;
    }

    sum = left + right;
    return true;
  }
}  // namespace

namespace doggo::gpu::deko
{
  const char * getGraphicsProgramStatusName( const GraphicsProgramStatus status ) noexcept
  {
    switch ( status )
    {
      case GraphicsProgramStatus::Success:
        return "Success";

      case GraphicsProgramStatus::InvalidArgument:
        return "Invalid Argument";

      case GraphicsProgramStatus::ShaderCodeSizeOverflow:
        return "Shader Code Size Overflow";

      case GraphicsProgramStatus::CodeMemoryCreationFailed:
        return "Code Memory Creation Failed";

      case GraphicsProgramStatus::CodeMemoryMappingFailed:
        return "Code Memory Mapping Failed";

      case GraphicsProgramStatus::InvalidVertexShader:
        return "Invalid Vertex Shader";

      case GraphicsProgramStatus::InvalidFragmentShader:
        return "Invalid Fragment Shader";

      case GraphicsProgramStatus::UnexpectedVertexStage:
        return "Unexpected Vertex Stage";

      case GraphicsProgramStatus::UnexpectedFragmentStage:
        return "Unexpected Fragment Stage";
    }

    return "Unknown";
  }

  GraphicsProgram::~GraphicsProgram()
  {
    finalize();
  }

  GraphicsProgramStatus GraphicsProgram::initialize( const dk::Device device,
                                                     const std::span<const std::uint8_t>
                                                         vertexBinary,
                                                     const std::span<const std::uint8_t>
                                                         fragmentBinary ) noexcept
  {
    if ( isInitialized() )
    {
      return GraphicsProgramStatus::Success;
    }

    finalize();

    if ( !device || vertexBinary.empty() || fragmentBinary.empty() )
    {
      return GraphicsProgramStatus::InvalidArgument;
    }

    std::uint64_t fragmentOffset = 0;
    if ( !alignUp( vertexBinary.size(), DK_SHADER_CODE_ALIGNMENT, fragmentOffset ) )
    {
      return GraphicsProgramStatus::ShaderCodeSizeOverflow;
    }

    std::uint64_t shaderCodeEnd = 0;
    std::uint64_t requiredSize  = 0;
    std::uint64_t memorySize    = 0;

    if ( !addWithoutOverflow( fragmentOffset, fragmentBinary.size(), shaderCodeEnd ) ||
         !addWithoutOverflow( shaderCodeEnd, DK_SHADER_CODE_UNUSABLE_SIZE, requiredSize ) ||
         !alignUp( requiredSize, DK_MEMBLOCK_ALIGNMENT, memorySize ) ||
         fragmentOffset > std::numeric_limits<std::uint32_t>::max() ||
         memorySize > std::numeric_limits<std::uint32_t>::max() )
    {
      return GraphicsProgramStatus::ShaderCodeSizeOverflow;
    }

    mCodeMemorySize = static_cast<std::uint32_t>( memorySize );
    mCodeMemory     = dk::MemBlockMaker{ device, mCodeMemorySize }
                      .setFlags( DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code )
                      .create();
    if ( !mCodeMemory )
    {
      mCodeMemorySize = 0;
      return GraphicsProgramStatus::CodeMemoryCreationFailed;
    }

    auto * const codeMemory = static_cast<std::uint8_t *>( mCodeMemory.getCpuAddr() );
    if ( !codeMemory )
    {
      finalize();
      return GraphicsProgramStatus::CodeMemoryMappingFailed;
    }

    std::memcpy( codeMemory, vertexBinary.data(), vertexBinary.size() );
    std::memcpy( codeMemory + fragmentOffset, fragmentBinary.data(), fragmentBinary.size() );

    dk::ShaderMaker{ mCodeMemory, 0 }.initialize( mVertexShader );
    if ( !mVertexShader.isValid() )
    {
      finalize();
      return GraphicsProgramStatus::InvalidVertexShader;
    }

    if ( mVertexShader.getStage() != DkStage_Vertex )
    {
      finalize();
      return GraphicsProgramStatus::UnexpectedVertexStage;
    }

    dk::ShaderMaker{ mCodeMemory, static_cast<std::uint32_t>( fragmentOffset ) }.initialize( mFragmentShader );
    if ( !mFragmentShader.isValid() )
    {
      finalize();
      return GraphicsProgramStatus::InvalidFragmentShader;
    }

    if ( mFragmentShader.getStage() != DkStage_Fragment )
    {
      finalize();
      return GraphicsProgramStatus::UnexpectedFragmentStage;
    }

    return GraphicsProgramStatus::Success;
  }

  void GraphicsProgram::finalize() noexcept
  {
    mFragmentShader = {};
    mVertexShader   = {};
    mCodeMemory     = nullptr;
    mCodeMemorySize = 0;
  }

  void GraphicsProgram::bind( dk::CmdBuf commandBuffer ) const noexcept
  {
    commandBuffer.bindShaders( DkStageFlag_GraphicsMask, { &mVertexShader, &mFragmentShader } );
  }

  bool GraphicsProgram::isInitialized() const noexcept
  {
    return static_cast<bool>( mCodeMemory ) && mVertexShader.isValid() && mFragmentShader.isValid();
  }

  std::uint32_t GraphicsProgram::codeMemorySize() const noexcept
  {
    return mCodeMemorySize;
  }
}  // namespace doggo::gpu::deko
