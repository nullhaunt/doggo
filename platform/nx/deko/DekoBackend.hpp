#pragma once

#include <cstdint>

#include <deko3d.hpp>

#include "RenderBackend.hpp"

namespace doggo::platform::nx::deko
{
    class DekoBackend final : public render::Backend
    {
      public:
        DekoBackend() = default;
        ~DekoBackend() override;

        [[nodiscard]] bool initialize() noexcept override;
        void               renderFrame() noexcept override;

      private:
        static constexpr std::size_t   sFramebufferCount     = 2;
        static constexpr std::uint32_t sFramebufferWidth     = 1280;
        static constexpr std::uint32_t sFramebufferHeight    = 720;
        static constexpr std::uint32_t sCommandMemorySize    = 16 * 1024;
        static constexpr std::uint32_t sShaderCodeMemorySize = 64 * 1024;

        dk::UniqueDevice mDevice;

        dk::UniqueMemBlock                       mFramebufferMemory;
        std::array<dk::Image, sFramebufferCount> mFramebuffers = {};
        dk::UniqueSwapchain                      mSwapChain;

        dk::UniqueMemBlock mCommandMemory;
        dk::UniqueCmdBuf   mCommandBuffer;

        std::array<DkCmdList, sFramebufferCount> mBindFramebufferCommands = {};
        DkCmdList                                mRenderCommands          = {};

        dk::UniqueMemBlock mShaderCodeMemory;
        std::uint32_t      mShaderCodeOffset = 0;

        dk::UniqueMemBlock mVertexMemory;
        dk::Shader         mVertexShader;

        dk::Shader mFragmentShader;

        [[nodiscard]] bool loadShader( dk::Shader & shader, const char * path ) noexcept;

        dk::UniqueQueue mQueue;

        bool mIsInitialized = false;
    };
} // namespace doggo::platform::nx::deko
