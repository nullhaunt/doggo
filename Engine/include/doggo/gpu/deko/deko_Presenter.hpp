#pragma once

#include "doggo/doggo_Macro.hpp"

#include <deko3d.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace doggo::gpu::deko
{
  struct PresentationExtent final
  {
      std::uint32_t width  = 0;
      std::uint32_t height = 0;
  };

  enum class PresentationStatus : std::uint8_t
  {
    Success,
    InvalidArgument,
    FramebufferSizeOverflow,
    FramebufferMemoryCreationFailed,
    SwapChainCreationFailed,
    CommandMemoryCreationFailed,
    CommandBufferCreationFailed,
    NotInitialized,
    FrameAlreadyActive,
    NoActiveFrame,
    FenceWaitFailed,
    InvalidImageSlot,
    QueueError,
  };

  struct PresentationReport final
  {
      PresentationStatus status        = PresentationStatus::Success;
      DkResult           deko_result   = DkResult_Success;
      std::uint32_t      context_index = 0;
      std::int32_t       image_slot    = -1;
  };

  struct PresentationFrame final
  {
      dk::CmdBuf         command_buffer;
      const dk::Image *  color_image   = nullptr;
      PresentationExtent extent        = {};
      std::uint32_t      context_index = 0;
      std::int32_t       image_slot    = -1;
  };

  [[nodiscard]] const char * getPresentationStatusName( PresentationStatus status ) noexcept;

  class Presenter final
  {
      DOGGO_DISALLOW_COPY( Presenter );
      DOGGO_DISALLOW_MOVE( Presenter );

    public:
      static constexpr std::size_t   FrameCount   = 3;
      static constexpr std::uint32_t SwapInterval = 2;

      Presenter() noexcept = default;
      ~Presenter();

      [[nodiscard]] PresentationReport initialize( dk::Device device, dk::Queue graphicsQueue, void * nativeWindow,
                                                   PresentationExtent extent ) noexcept;
      [[nodiscard]] PresentationReport finalize() noexcept;

      [[nodiscard]] PresentationReport beginFrame( PresentationFrame & frame ) noexcept;
      [[nodiscard]] PresentationReport endFrame() noexcept;

      [[nodiscard]] bool               isInitialized() const noexcept;
      [[nodiscard]] PresentationExtent extent() const noexcept;

    private:
      static constexpr std::uint32_t CommandMemoryPerFrame = 1024u * 1024u;

      struct FrameContext final
      {
          dk::UniqueCmdBuf command_buffer;
          dk::Fence        completion_fence;
          bool             is_in_flight = false;
      };

      void resetResources() noexcept;

      dk::Queue                            mGraphicsQueue;
      dk::UniqueMemBlock                   mFramebufferMemory;
      std::array<dk::Image, FrameCount>    mFramebufferImages = {};
      dk::UniqueSwapchain                  mSwapChain;
      dk::UniqueMemBlock                   mCommandMemory;
      std::array<FrameContext, FrameCount> mFrames             = {};
      PresentationExtent                   mExtent             = {};
      std::size_t                          mNextContextIndex   = 0;
      std::size_t                          mActiveContextIndex = 0;
      std::int32_t                         mActiveImageSlot    = -1;
      bool                                 mHasActiveFrame     = false;
  };
}  // namespace doggo::gpu::deko
