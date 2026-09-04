#include "DekoBackend.hpp"

#include <limits>

#include <switch.h>

#include "diagnostics/Assert.hpp"

namespace doggo::platform::nx::deko
{
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

        mCommandMemory = dk::UniqueMemBlock{ dk::MemBlockMaker{ mDevice, sCommandMemorySize }
                                                 .setFlags( DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached )
                                                 .create() };
        if ( !mCommandMemory )
        {
            return false;
        }

        mCommandBuffer = dk::UniqueCmdBuf{ dk::CmdBufMaker{ mDevice }.create() };
        if ( !mCommandBuffer )
        {
            return false;
        }

        mCommandBuffer.addMemory( mCommandMemory, 0, sCommandMemorySize );

        for ( std::size_t i = 0; i < sFramebufferCount; ++i )
        {
            dk::ImageView                      imageView{ mFramebuffers[ i ] };
            std::array<DkImageView const *, 1> colorTargets{ &imageView };

            mCommandBuffer.bindRenderTargets( colorTargets );
            mBindFramebufferCommands[ i ] = mCommandBuffer.finishList();
        }

        const DkViewport viewport{
            0.0f, 0.0f, static_cast<float>( sFramebufferWidth ), static_cast<float>( sFramebufferHeight ), 0.0f, 1.0f };

        const DkScissor scissor{ 0, 0, sFramebufferWidth, sFramebufferHeight };

        mCommandBuffer.setViewports( 0, viewport );
        mCommandBuffer.setScissors( 0, scissor );

        mCommandBuffer.clearColor( 0, DkColorMask_RGBA, 0.08f, 0.12f, 0.18f, 1.0f );
        mClearCommands = mCommandBuffer.finishList();

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
        mQueue.submitCommands( mClearCommands );
        mQueue.presentImage( mSwapChain, slot );
    }
} // namespace doggo::platform::nx::deko
