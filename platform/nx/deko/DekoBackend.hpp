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

        [[nodiscard]] render::MeshHandle createMesh( const render::MeshData & meshData ) noexcept override;

        void renderFrame( const render::FrameInfo &           frameInfo,
                          std::span<const render::DrawPacket> draws ) noexcept override;

      private:
        static constexpr std::size_t sMeshCapacity = 8;

        struct MeshResource
        {
            DekoMemorySlice mVertexMemory;
            DekoMemorySlice mIndexMemory;
            std::uint32_t   mIndexCount = 0;
        };

        static constexpr std::size_t   sFramebufferCount  = 2;
        static constexpr std::uint32_t sFramebufferWidth  = 1280;
        static constexpr std::uint32_t sFramebufferHeight = 720;

        static constexpr std::uint32_t sCommandMemorySize = 64 * 1024;
        static_assert( sCommandMemorySize % DK_CMDMEM_ALIGNMENT == 0 );

        static constexpr std::uint32_t sShaderCodeMemorySize = 64 * 1024;
        static constexpr std::uint32_t sDataMemorySize       = 1024 * 1024;

        static constexpr std::size_t   sBootstrapDrawCapacity = 16;
        static constexpr std::uint32_t sTransformStride       = DK_UNIFORM_BUF_ALIGNMENT;
        static_assert( sizeof( render::DrawData ) <= sTransformStride );

        dk::UniqueDevice mDevice;

        DekoMemoryArena mDataMemory;

        dk::Image mDepthBuffer;

        dk::UniqueMemBlock                       mFramebufferMemory;
        std::array<dk::Image, sFramebufferCount> mFramebuffers = {};
        dk::UniqueSwapchain                      mSwapChain;

        dk::UniqueCmdBuf mCommandBuffer;
        DekoMemorySlice  mCommandMemory;

        std::array<DkCmdList, sFramebufferCount>                                     mBindFramebufferCommands = {};
        DkCmdList                                                                    mRenderStateCommands     = {};
        std::array<std::array<DkCmdList, sBootstrapDrawCapacity>, sFramebufferCount> mBindTransformCommands   = {};

        dk::UniqueMemBlock mShaderCodeMemory;
        std::uint32_t      mShaderCodeOffset = 0;

        dk::Shader mVertexShader;
        dk::Shader mFragmentShader;

        std::array<MeshResource, sMeshCapacity> mMeshes           = {};
        std::array<DkCmdList, sMeshCapacity>    mMeshDrawCommands = {};
        std::size_t                             mMeshCount        = 0;

        std::array<DekoMemorySlice, sFramebufferCount> mTransformMemory = {};

        std::uint32_t mIndexCount = 0;

        [[nodiscard]] bool loadShader( dk::Shader & shader, const char * path ) noexcept;

      private:
        dk::UniqueQueue mQueue;

        bool mIsInitialized = false;
    };
} // namespace doggo::platform::nx::deko
