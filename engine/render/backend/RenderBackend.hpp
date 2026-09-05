#pragma once

#include <array>
#include <chrono>
#include <span>

namespace doggo::render
{
    struct FrameInfo
    {
        std::uint64_t            mFrameIndex  = 0;
        std::chrono::nanoseconds mDeltaTime   = {};
        std::chrono::nanoseconds mElapsedTime = {};
    };

    struct MeshVertex
    {
        float mPosition[ 3 ];
        float mColor[ 3 ];
    };

    struct MeshData
    {
        std::span<const MeshVertex>    mVertices;
        std::span<const std::uint16_t> mIndices;
    };

    struct MeshHandle
    {
        static constexpr std::uint32_t sInvalidIndex = 0xFFFFFFFFu;

        std::uint32_t mIndex = sInvalidIndex;

        [[nodiscard]] bool isValid() const noexcept
        {
            return mIndex != sInvalidIndex;
        }
    };

    using MeshId = std::uint32_t;

    struct alignas( 16 ) DrawData
    {
        std::array<float, 16> mModelViewProjection = {};
    };

    static_assert( sizeof( DrawData ) == 64 );

    struct DrawRequest
    {
        MeshId   mMeshId = 0;
        DrawData mDrawData;
    };

    struct DrawPacket
    {
        MeshHandle mMesh;
        DrawData   mDrawData;
    };

    class Backend
    {
      public:
        Backend()          = default;
        virtual ~Backend() = default;

        Backend( const Backend & )             = delete;
        Backend & operator=( const Backend & ) = delete;

        Backend( Backend && )             = delete;
        Backend & operator=( Backend && ) = delete;

        [[nodiscard]] virtual bool initialize() noexcept = 0;

        [[nodiscard]] virtual MeshHandle createMesh( const MeshData & meshData ) noexcept = 0;

        virtual void renderFrame( const FrameInfo & frameInfo, std::span<const DrawPacket> draws ) noexcept = 0;
    };

    class NullBackend : public Backend
    {
      public:
        [[nodiscard]] bool initialize() noexcept override;
        void renderFrame( const FrameInfo & frameInfo, std::span<const DrawPacket> draws ) noexcept override;
    };
} // namespace doggo::render
