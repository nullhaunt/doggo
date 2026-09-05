#pragma once

#include "RenderSource.hpp"

namespace doggo::render
{
    class Renderer final
    {
      public:
        Renderer( Backend & backend, Source & source ) noexcept;

        [[nodiscard]] bool initialize() noexcept;
        void               renderFrame( const FrameInfo & frameInfo ) noexcept;

      private:
        static constexpr std::size_t sMeshCapacity = 8;
        static constexpr std::size_t sDrawCapacity = 16;

        Backend & mBackend;
        Source &  mSource;

        std::array<MeshHandle, sMeshCapacity> mMeshes      = {};
        std::array<DrawPacket, sDrawCapacity> mDrawPackets = {};

        std::size_t mMeshCount = 0;
    };
} // namespace doggo::render
