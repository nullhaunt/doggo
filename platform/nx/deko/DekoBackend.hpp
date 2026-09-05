#pragma once

#include <cstdint>

#include <deko3d.hpp>

#include "DekoMemoryArena.hpp"
#include "RenderBackend.hpp"

namespace doggo::platform::nx::deko
{
    class DekoBackend final : public render::Backend
    {
      public:
        DekoBackend() = default;
        ~DekoBackend() override;

        [[nodiscard]] bool initialize() noexcept override;
        void               renderFrame( const render::FrameInfo & frameInfo ) noexcept override;

      private:
        static constexpr std::size_t   sFramebufferCount  = 2;
        static constexpr std::uint32_t sFramebufferWidth  = 1280;
        static constexpr std::uint32_t sFramebufferHeight = 720;

        static constexpr std::uint32_t sCommandMemorySize = 16 * 1024;
        static_assert( sCommandMemorySize % DK_CMDMEM_ALIGNMENT == 0 );

        static constexpr std::uint32_t sShaderCodeMemorySize = 64 * 1024;
        static constexpr std::uint32_t sDataMemorySize       = 1024 * 1024;

        dk::UniqueDevice mDevice;

        DekoMemoryArena mDataMemory;

        dk::Image mDepthBuffer;

        dk::UniqueMemBlock                       mFramebufferMemory;
        std::array<dk::Image, sFramebufferCount> mFramebuffers = {};
        dk::UniqueSwapchain                      mSwapChain;

        dk::UniqueCmdBuf mCommandBuffer;
        DekoMemorySlice  mCommandMemory;

        std::array<DkCmdList, sFramebufferCount> mBindFramebufferCommands = {};
        DkCmdList                                mRenderCommands          = {};

        dk::UniqueMemBlock mShaderCodeMemory;
        std::uint32_t      mShaderCodeOffset = 0;

        dk::Shader mVertexShader;
        dk::Shader mFragmentShader;

        DekoMemorySlice                                mVertexMemory;
        DekoMemorySlice                                mIndexMemory;
        std::array<DekoMemorySlice, sFramebufferCount> mTransformMemory = {};

        [[nodiscard]] bool loadShader( dk::Shader & shader, const char * path ) noexcept;

        dk::UniqueQueue mQueue;

        bool mIsInitialized = false;
    };
} // namespace doggo::platform::nx::deko
