#include "DekoBackend.hpp"

#include <cstdio>
#include <cstring>
#include <limits>

#include <switch.h>

#include "diagnostics/Assert.hpp"

namespace doggo::platform::nx::deko
{
    namespace
    {
        struct Vertex
        {
            float mPosition[ 2 ];
            float mColor[ 3 ];
        };

        constexpr std::array<Vertex, 3> TriangleVertices{ {
            { .mPosition = { 0.0f, 0.7f }, .mColor = { 1.0f, 0.2f, 0.2f } },
            { .mPosition = { -0.7f, -0.7f }, .mColor = { 0.2f, 1.0f, 0.2f } },
            { .mPosition = { 0.7f, -0.7f }, .mColor = { 0.2f, 0.4f, 1.0f } },
        } };
    } // namespace

    DekoBackend::~DekoBackend()
    {
        if ( mQueue )
        {
            mQueue.waitIdle();
        }
    }

    bool DekoBackend::initialize() noexcept
    {
        if ( mIsInitialized )
        {
            return true;
        }

        mDevice = dk::UniqueDevice{ dk::DeviceMaker{}.create() };
        if ( !mDevice )
        {
            return false;
        }

        if ( !mDataMemory.initialize(
                 mDevice, sDataMemorySize, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached ) )
        {
            return false;
        }

        dk::ImageLayout framebufferLayout;

        dk::ImageLayoutMaker{ mDevice }
            .setFlags( DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression )
            .setFormat( DkImageFormat_RGBA8_Unorm )
            .setDimensions( sFramebufferWidth, sFramebufferHeight )
            .initialize( framebufferLayout );

        const std::uint64_t framebufferAlignment = framebufferLayout.getAlignment();
        if ( framebufferAlignment == 0 )
        {
            return false;
        }

        const std::uint64_t framebufferStride =
            ( framebufferLayout.getSize() + framebufferAlignment - 1 ) / framebufferAlignment * framebufferAlignment;

        const std::uint64_t totalFramebufferSize = framebufferStride * sFramebufferCount;
        if ( totalFramebufferSize > std::numeric_limits<std::uint32_t>::max() )
        {
            return false;
        }

        mFramebufferMemory =
            dk::UniqueMemBlock{ dk::MemBlockMaker{ mDevice, static_cast<std::uint32_t>( totalFramebufferSize ) }
                                    .setFlags( DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image )
                                    .create() };

        if ( !mFramebufferMemory )
        {
            return false;
        }

        mShaderCodeMemory = dk::UniqueMemBlock{
            dk::MemBlockMaker{ mDevice, sShaderCodeMemorySize }
                .setFlags( DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code )
                .create() };

        if ( !mShaderCodeMemory )
        {
            return false;
        }

        DkVtxAttribState positionAttribute = {};
        positionAttribute.bufferId         = 0;
        positionAttribute.offset           = offsetof( Vertex, mPosition );
        positionAttribute.size             = DkVtxAttribSize_2x32;
        positionAttribute.type             = DkVtxAttribType_Float;

        DkVtxAttribState colorAttribute = {};
        colorAttribute.bufferId         = 0;
        colorAttribute.offset           = offsetof( Vertex, mColor );
        colorAttribute.size             = DkVtxAttribSize_3x32;
        colorAttribute.type             = DkVtxAttribType_Float;

        const std::array     vertexAttributes   = { positionAttribute, colorAttribute };
        constexpr std::array vertexBufferStates = { DkVtxBufferState{ .stride = sizeof( Vertex ), .divisor = 0 } };

        const Result romfsResult = romfsInit();

        if ( R_FAILED( romfsResult ) )
        {
            return false;
        }

        const bool shadersLoaded = loadShader( mVertexShader, "romfs:/shaders/triangle.vert.dksh" ) &&
                                   loadShader( mFragmentShader, "romfs:/shaders/triangle.frag.dksh" );

        romfsExit();

        if ( !shadersLoaded )
        {
            return false;
        }

        constexpr std::uint32_t VertexBufferAlignment = 16;
        const auto vertexMemory = mDataMemory.allocate( sizeof( TriangleVertices ), VertexBufferAlignment );

        if ( !vertexMemory )
        {
            return false;
        }

        mVertexMemory = *vertexMemory;
        std::memcpy( mDataMemory.getCpuAddress( mVertexMemory ), TriangleVertices.data(), sizeof( TriangleVertices ) );

        std::array<DkImage const *, sFramebufferCount> swapChainImages = {};

        for ( std::size_t i = 0; i < sFramebufferCount; ++i )
        {
            mFramebuffers[ i ].initialize(
                framebufferLayout, mFramebufferMemory, static_cast<std::uint32_t>( i * framebufferStride ) );

            swapChainImages[ i ] = &mFramebuffers[ i ];
        }

        mSwapChain =
            dk::UniqueSwapchain{ dk::SwapchainMaker{ mDevice, nwindowGetDefault(), swapChainImages }.create() };

        if ( !mSwapChain )
        {
            return false;
        }

        mCommandBuffer = dk::UniqueCmdBuf{ dk::CmdBufMaker{ mDevice }.create() };

        if ( !mCommandBuffer )
        {
            return false;
        }

        const auto commandMemory = mDataMemory.allocate( sCommandMemorySize, DK_CMDMEM_ALIGNMENT );

        if ( !commandMemory )
        {
            return false;
        }

        mCommandMemory = *commandMemory;

        mCommandBuffer.addMemory( mDataMemory.getMemoryBlock(), mCommandMemory.mOffset, mCommandMemory.mSize );

        for ( std::size_t i = 0; i < sFramebufferCount; ++i )
        {
            dk::ImageView                      imageView{ mFramebuffers[ i ] };
            std::array<DkImageView const *, 1> colorTargets{ &imageView };

            mCommandBuffer.bindRenderTargets( colorTargets );
            mBindFramebufferCommands[ i ] = mCommandBuffer.finishList();
        }

        constexpr DkViewport viewport{ .x      = 0.0f,
                                       .y      = 0.0f,
                                       .width  = static_cast<float>( sFramebufferWidth ),
                                       .height = static_cast<float>( sFramebufferHeight ),
                                       .near   = 0.0f,
                                       .far    = 1.0f };

        constexpr DkScissor scissor{ .x = 0, .y = 0, .width = sFramebufferWidth, .height = sFramebufferHeight };

        mCommandBuffer.setViewports( 0, viewport );
        mCommandBuffer.setScissors( 0, scissor );

        mCommandBuffer.clearColor( 0, DkColorMask_RGBA, 0.08f, 0.12f, 0.18f, 1.0f );

        const std::array<DkShader const *, 2> shaders{ &mVertexShader, &mFragmentShader };

        dk::RasterizerState rasterizerState;
        dk::ColorState      colorState;
        dk::ColorWriteState colorWriteState;

        mCommandBuffer.bindShaders( DkStageFlag_GraphicsMask, shaders );
        mCommandBuffer.bindRasterizerState( rasterizerState );
        mCommandBuffer.bindColorState( colorState );
        mCommandBuffer.bindColorWriteState( colorWriteState );

        mCommandBuffer.bindVtxAttribState( vertexAttributes );
        mCommandBuffer.bindVtxBufferState( vertexBufferStates );

        mCommandBuffer.bindVtxBuffer( 0, mDataMemory.getGpuAddress( mVertexMemory ), mVertexMemory.mSize );

        mCommandBuffer.draw( DkPrimitive_Triangles, 3, 1, 0, 0 );

        mRenderCommands = mCommandBuffer.finishList();

        mQueue = dk::UniqueQueue{ dk::QueueMaker{ mDevice }.setFlags( DkQueueFlags_Graphics ).create() };

        if ( !mQueue )
        {
            return false;
        }

        mIsInitialized = true;
        return true;
    }

    void DekoBackend::renderFrame() noexcept
    {
        DOGGO_ASSERT( mIsInitialized );

        if ( !mIsInitialized )
        {
            return;
        }

        const int slot = mQueue.acquireImage( mSwapChain );
        DOGGO_ASSERT( slot >= 0 );
        DOGGO_ASSERT( static_cast<std::size_t>( slot ) < sFramebufferCount );

        if ( slot < 0 || static_cast<std::size_t>( slot ) >= sFramebufferCount )
        {
            return;
        }

        mQueue.submitCommands( mBindFramebufferCommands[ static_cast<std::size_t>( slot ) ] );
        mQueue.submitCommands( mRenderCommands );
        mQueue.presentImage( mSwapChain, slot );
    }

    bool DekoBackend::loadShader( dk::Shader & shader, const char * path ) noexcept
    {
        std::FILE * file = std::fopen( path, "rb" );
        if ( !file )
        {
            return false;
        }

        if ( std::fseek( file, 0, SEEK_END ) != 0 )
        {
            std::fclose( file );
            return false;
        }

        const long fileSize = std::ftell( file );
        if ( fileSize <= 0 || static_cast<unsigned long>( fileSize ) > sShaderCodeMemorySize )
        {
            std::fclose( file );
            return false;
        }

        std::rewind( file );

        const auto          size        = static_cast<std::uint32_t>( fileSize );
        const std::uint32_t alignedSize = ( size + DK_SHADER_CODE_ALIGNMENT - 1 ) & ~( DK_SHADER_CODE_ALIGNMENT - 1 );

        if ( alignedSize > sShaderCodeMemorySize - mShaderCodeOffset )
        {
            std::fclose( file );
            return false;
        }

        auto *            destination = static_cast<std::byte *>( mShaderCodeMemory.getCpuAddr() ) + mShaderCodeOffset;
        const std::size_t bytesRead   = std::fread( destination, 1, size, file );
        std::fclose( file );

        if ( bytesRead != size )
        {
            return false;
        }

        dk::ShaderMaker{ mShaderCodeMemory, mShaderCodeOffset }.initialize( shader );

        if ( !shader.isValid() )
        {
            return false;
        }

        mShaderCodeOffset += alignedSize;
        return true;
    }
} // namespace doggo::platform::nx::deko
