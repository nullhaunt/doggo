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

    struct alignas( 16 ) DrawData
    {
        std::array<float, 16> mModelViewProjection = {};
    };

    static_assert( sizeof( DrawData ) == 64 );

    class Backend
    {
      public:
        Backend()          = default;
        virtual ~Backend() = default;

        Backend( const Backend & )             = delete;
        Backend & operator=( const Backend & ) = delete;

        Backend( Backend && )             = delete;
        Backend & operator=( Backend && ) = delete;

        [[nodiscard]] virtual bool initialize( const MeshData & meshData ) noexcept                               = 0;
        virtual void               renderFrame( const FrameInfo & frameInfo, const DrawData & drawData ) noexcept = 0;
    };

    class NullBackend : public Backend
    {
      public:
        [[nodiscard]] bool initialize( const MeshData & meshData ) noexcept override;
        void               renderFrame( const FrameInfo & frameInfo, const DrawData & drawData ) noexcept override;
    };
} // namespace doggo::render
