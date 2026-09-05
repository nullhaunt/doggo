#pragma once

#include "RenderSource.hpp"

namespace doggo::sample::mesh
{
    class MeshSample final : public render::Source
    {
      public:
        MeshSample() noexcept;

        [[nodiscard]] std::span<const render::DrawRequest> buildDrawRequests(
            const render::FrameInfo & frameInfo ) noexcept override;
        [[nodiscard]] std::span<const render::MeshData> getMeshes() const noexcept override;

      private:
        static constexpr std::size_t sMeshCount = 2;
        static constexpr std::size_t sDrawCount = 9;

        std::array<render::MeshData, sMeshCount>    mMeshes = {};
        std::array<render::DrawRequest, sDrawCount> mDraws  = {};
    };
} // namespace doggo::sample::mesh
