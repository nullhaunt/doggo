#include "doggo/gpu/deko/deko_Presenter.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace
{
  [[nodiscard]] bool alignUp( const std::uint64_t value, const std::uint32_t alignment,
                              std::uint64_t & alignedValue ) noexcept
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

  [[nodiscard]] doggo::gpu::deko::PresentationReport makeReport( const doggo::gpu::deko::PresentationStatus status,
                                                                 const DkResult      dekoResult   = DkResult_Success,
                                                                 const std::uint32_t contextIndex = 0,
                                                                 const std::int32_t  imageSlot    = -1 ) noexcept
  {
    return {
        .status        = status,
        .deko_result   = dekoResult,
        .context_index = contextIndex,
        .image_slot    = imageSlot,
    };
  }
}  // namespace

namespace doggo::gpu::deko
{
  const char * getPresentationStatusName( const PresentationStatus status ) noexcept
  {
    switch ( status )
    {
      case PresentationStatus::Success:
        return "Success";

      case PresentationStatus::InvalidArgument:
        return "Invalid Argument";

      case PresentationStatus::FramebufferSizeOverflow:
        return "Framebuffer Size Overflow";

      case PresentationStatus::FramebufferMemoryCreationFailed:
        return "Framebuffer Memory Creation Failed";

      case PresentationStatus::SwapChainCreationFailed:
        return "Swap Chain Creation Failed";

      case PresentationStatus::CommandMemoryCreationFailed:
        return "Command Memory Creation Failed";

      case PresentationStatus::CommandBufferCreationFailed:
        return "Command Buffer Creation Failed";

      case PresentationStatus::NotInitialized:
        return "Not Initialized";

      case PresentationStatus::FrameAlreadyActive:
        return "Frame Already Active";

      case PresentationStatus::NoActiveFrame:
        return "No Active Frame";

      case PresentationStatus::FenceWaitFailed:
        return "Fence Wait Failed";

      case PresentationStatus::InvalidImageSlot:
        return "Invalid Image Slot";

      case PresentationStatus::QueueError:
        return "Queue Error";
    }

    return "Unknown";
  }

  Presenter::~Presenter()
  {
    ( void )finalize();
  }

  PresentationReport Presenter::initialize( const dk::Device         device,
                                            const dk::Queue          graphicsQueue,
                                            void * const             nativeWindow,
                                            const PresentationExtent extent ) noexcept
  {
    if ( isInitialized() )
    {
      return makeReport( PresentationStatus::Success );
    }

    ( void )finalize();

    if ( !device || !graphicsQueue || !nativeWindow || extent.width == 0 || extent.height == 0 )
    {
      return makeReport( PresentationStatus::InvalidArgument );
    }

    dk::ImageLayout framebufferLayout;
    dk::ImageLayoutMaker{ device }
        .setFlags( DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression )
        .setFormat( DkImageFormat_RGBA8_Unorm )
        .setDimensions( extent.width, extent.height )
        .initialize( framebufferLayout );

    const std::uint32_t imageAlignment =
        std::max<std::uint32_t>( framebufferLayout.getAlignment(), DK_MEMBLOCK_ALIGNMENT );
    std::uint64_t imageStride = 0;
    if ( !alignUp( framebufferLayout.getSize(), imageAlignment, imageStride ) ||
         imageStride > std::numeric_limits<std::uint32_t>::max() ||
         imageStride > std::numeric_limits<std::uint32_t>::max() / FrameCount )
    {
      return makeReport( PresentationStatus::FramebufferSizeOverflow );
    }

    const auto framebufferMemorySize = static_cast<std::uint32_t>( imageStride * FrameCount );
    mFramebufferMemory               = dk::MemBlockMaker{ device, framebufferMemorySize }
                             .setFlags( DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image )
                             .create();
    if ( !mFramebufferMemory )
    {
      resetResources();
      return makeReport( PresentationStatus::FramebufferMemoryCreationFailed );
    }

    std::array<const DkImage *, FrameCount> swapChainImages = {};
    for ( std::size_t index = 0; index < FrameCount; ++index )
    {
      const auto imageOffset = static_cast<std::uint32_t>( imageStride * index );
      mFramebufferImages[ index ].initialize( framebufferLayout, mFramebufferMemory, imageOffset );
      swapChainImages[ index ] = &mFramebufferImages[ index ];
    }

    mSwapChain = dk::SwapchainMaker{ device, nativeWindow, swapChainImages }.create();
    if ( !mSwapChain )
    {
      resetResources();
      return makeReport( PresentationStatus::SwapChainCreationFailed );
    }
    mSwapChain.setSwapInterval( SwapInterval );

    static_assert( CommandMemoryPerFrame % DK_CMDMEM_ALIGNMENT == 0 );
    static_assert( CommandMemoryPerFrame % DK_MEMBLOCK_ALIGNMENT == 0 );
    static_assert( CommandMemoryPerFrame <= std::numeric_limits<std::uint32_t>::max() / FrameCount );
    constexpr std::uint32_t commandMemorySize = CommandMemoryPerFrame * FrameCount;
    mCommandMemory                            = dk::MemBlockMaker{ device, commandMemorySize }
                         .setFlags( DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached )
                         .create();
    if ( !mCommandMemory )
    {
      resetResources();
      return makeReport( PresentationStatus::CommandMemoryCreationFailed );
    }

    for ( std::size_t index = 0; index < FrameCount; ++index )
    {
      FrameContext & frame = mFrames[ index ];
      // The fence guards both this command buffer and its fixed memory slice.
      frame.command_buffer = dk::CmdBufMaker{ device }.create();
      if ( !frame.command_buffer )
      {
        resetResources();
        return makeReport( PresentationStatus::CommandBufferCreationFailed,
                           DkResult_Success,
                           static_cast<std::uint32_t>( index ) );
      }

      frame.command_buffer.addMemory( mCommandMemory,
                                      static_cast<std::uint32_t>( index ) * CommandMemoryPerFrame,
                                      CommandMemoryPerFrame );
      frame.completion_fence = {};
      frame.is_in_flight     = false;
    }

    mGraphicsQueue = graphicsQueue;
    mExtent        = extent;
    return makeReport( PresentationStatus::Success );
  }

  PresentationReport Presenter::finalize() noexcept
  {
    PresentationReport report = makeReport( PresentationStatus::Success );

    if ( mGraphicsQueue && mGraphicsQueue.isInErrorState() )
    {
      report = makeReport( PresentationStatus::QueueError );
    }
    else
    {
      for ( std::size_t index = 0; index < FrameCount; ++index )
      {
        FrameContext & frame = mFrames[ index ];
        if ( !frame.is_in_flight )
        {
          continue;
        }

        const DkResult waitResult = frame.completion_fence.wait();
        if ( waitResult != DkResult_Success && report.status == PresentationStatus::Success )
        {
          report = makeReport( PresentationStatus::FenceWaitFailed, waitResult, static_cast<std::uint32_t>( index ) );
        }
        frame.is_in_flight = false;
      }
    }

    resetResources();
    return report;
  }

  PresentationReport Presenter::beginFrame( PresentationFrame & frame ) noexcept
  {
    frame = {};

    if ( !isInitialized() )
    {
      return makeReport( PresentationStatus::NotInitialized );
    }

    if ( mHasActiveFrame )
    {
      return makeReport( PresentationStatus::FrameAlreadyActive );
    }

    if ( mGraphicsQueue.isInErrorState() )
    {
      return makeReport( PresentationStatus::QueueError );
    }

    FrameContext & context = mFrames[ mNextContextIndex ];
    if ( context.is_in_flight )
    {
      const DkResult waitResult = context.completion_fence.wait();
      if ( waitResult != DkResult_Success )
      {
        return makeReport( PresentationStatus::FenceWaitFailed,
                           waitResult,
                           static_cast<std::uint32_t>( mNextContextIndex ) );
      }
      context.is_in_flight = false;
    }

    context.command_buffer.clear();
    const std::int32_t imageSlot = mGraphicsQueue.acquireImage( mSwapChain );
    if ( imageSlot < 0 || std::cmp_greater_equal( imageSlot, FrameCount ) )
    {
      return makeReport( PresentationStatus::InvalidImageSlot,
                         DkResult_Success,
                         static_cast<std::uint32_t>( mNextContextIndex ),
                         imageSlot );
    }

    mActiveContextIndex = mNextContextIndex;
    mActiveImageSlot    = imageSlot;
    mHasActiveFrame     = true;

    frame.command_buffer = context.command_buffer;
    frame.color_image    = &mFramebufferImages[ static_cast<std::size_t>( imageSlot ) ];
    frame.extent         = mExtent;
    frame.context_index  = static_cast<std::uint32_t>( mActiveContextIndex );
    frame.image_slot     = imageSlot;
    return makeReport( PresentationStatus::Success, DkResult_Success, frame.context_index, frame.image_slot );
  }

  PresentationReport Presenter::endFrame() noexcept
  {
    if ( !isInitialized() )
    {
      return makeReport( PresentationStatus::NotInitialized );
    }

    if ( !mHasActiveFrame )
    {
      return makeReport( PresentationStatus::NoActiveFrame );
    }

    FrameContext &  context  = mFrames[ mActiveContextIndex ];
    const DkCmdList commands = context.command_buffer.finishList();
    mGraphicsQueue.submitCommands( commands );
    mGraphicsQueue.signalFence( context.completion_fence );
    // Presentation flushes the pending queue batch, including the fence signal.
    mGraphicsQueue.presentImage( mSwapChain, mActiveImageSlot );
    context.is_in_flight = true;

    const auto completedContext = static_cast<std::uint32_t>( mActiveContextIndex );
    const auto completedImage   = mActiveImageSlot;
    mNextContextIndex           = ( mActiveContextIndex + 1 ) % FrameCount;
    mHasActiveFrame             = false;
    mActiveImageSlot            = -1;

    if ( mGraphicsQueue.isInErrorState() )
    {
      return makeReport( PresentationStatus::QueueError, DkResult_Success, completedContext, completedImage );
    }

    return makeReport( PresentationStatus::Success, DkResult_Success, completedContext, completedImage );
  }

  bool Presenter::isInitialized() const noexcept
  {
    if ( !mGraphicsQueue || !mFramebufferMemory || !mSwapChain || !mCommandMemory )
    {
      return false;
    }

    return std::ranges::all_of( mFrames,
                                []( const FrameContext & frame )
                                {
                                  return static_cast<bool>( frame.command_buffer );
                                } );
  }

  PresentationExtent Presenter::extent() const noexcept
  {
    return mExtent;
  }

  void Presenter::resetResources() noexcept
  {
    mHasActiveFrame     = false;
    mActiveImageSlot    = -1;
    mActiveContextIndex = 0;

    for ( FrameContext & frame : mFrames )
    {
      frame.command_buffer   = nullptr;
      frame.completion_fence = {};
      frame.is_in_flight     = false;
    }

    mCommandMemory     = nullptr;
    mSwapChain         = nullptr;
    mFramebufferMemory = nullptr;
    mGraphicsQueue     = {};
    mExtent            = {};
    mNextContextIndex  = 0;
  }
}  // namespace doggo::gpu::deko
