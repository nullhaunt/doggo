#include "doggo/gpu/deko/deko_GraphicsProgram.hpp"

#include "doggo/gpu/deko/deko_MemoryArena.hpp"

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

      case GraphicsProgramStatus::CodeArenaNotInitialized:
        return "Code Arena Not Initialized";

      case GraphicsProgramStatus::ShaderCodeSizeOverflow:
        return "Shader Code Size Overflow";

      case GraphicsProgramStatus::CodeArenaAllocationFailed:
        return "Code Arena Allocation Failed";

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

  GraphicsProgramStatus GraphicsProgram::initialize( MemoryArena & codeArena,
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

    if ( vertexBinary.empty() || fragmentBinary.empty() )
    {
      return GraphicsProgramStatus::InvalidArgument;
    }

    if ( !codeArena.isInitialized() )
    {
      return GraphicsProgramStatus::CodeArenaNotInitialized;
    }

    if ( ( codeArena.snapshot().flags & DkMemBlockFlags_Code ) == 0 )
    {
      return GraphicsProgramStatus::InvalidArgument;
    }

    std::uint64_t fragmentOffset = 0;
    if ( !alignUp( vertexBinary.size(), DK_SHADER_CODE_ALIGNMENT, fragmentOffset ) )
    {
      return GraphicsProgramStatus::ShaderCodeSizeOverflow;
    }

    std::uint64_t requiredSize = 0;
    if ( !addWithoutOverflow( fragmentOffset, fragmentBinary.size(), requiredSize ) ||
         fragmentOffset > std::numeric_limits<std::uint32_t>::max() ||
         requiredSize > std::numeric_limits<std::uint32_t>::max() )
    {
      return GraphicsProgramStatus::ShaderCodeSizeOverflow;
    }

    const ArenaAllocationResult allocationResult =
        codeArena.allocate( static_cast<std::uint32_t>( requiredSize ), DK_SHADER_CODE_ALIGNMENT );
    if ( allocationResult.status != ArenaAllocationStatus::Success )
    {
      return GraphicsProgramStatus::CodeArenaAllocationFailed;
    }

    mCodeArena      = &codeArena;
    mCodeAllocation = allocationResult.allocation;
    mCodeMemorySize = static_cast<std::uint32_t>( requiredSize );

    const std::span<std::byte> codeMemory = codeArena.cpuSpan( allocationResult.allocation );
    if ( codeMemory.size() != requiredSize )
    {
      finalize();
      return GraphicsProgramStatus::CodeMemoryMappingFailed;
    }

    std::memcpy( codeMemory.data(), vertexBinary.data(), vertexBinary.size() );
    std::memcpy( codeMemory.data() + fragmentOffset, fragmentBinary.data(), fragmentBinary.size() );

    dk::ShaderMaker{ codeArena.memoryBlock(), mCodeAllocation.offset }.initialize( mVertexShader );
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

    dk::ShaderMaker{ codeArena.memoryBlock(), mCodeAllocation.offset + static_cast<std::uint32_t>( fragmentOffset ) }
        .initialize( mFragmentShader );
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

    if ( mCodeArena && mCodeArena->owns( mCodeAllocation ) )
    {
      ( void )mCodeArena->release( mCodeAllocation );
    }

    mCodeArena      = nullptr;
    mCodeAllocation = {};
    mCodeMemorySize = 0;
  }

  void GraphicsProgram::bind( dk::CmdBuf commandBuffer ) const noexcept
  {
    commandBuffer.bindShaders( DkStageFlag_GraphicsMask, { &mVertexShader, &mFragmentShader } );
  }

  bool GraphicsProgram::isInitialized() const noexcept
  {
    return mCodeArena && mCodeArena->owns( mCodeAllocation ) && mVertexShader.isValid() && mFragmentShader.isValid();
  }

  std::uint32_t GraphicsProgram::codeMemorySize() const noexcept
  {
    return mCodeMemorySize;
  }
}  // namespace doggo::gpu::deko
