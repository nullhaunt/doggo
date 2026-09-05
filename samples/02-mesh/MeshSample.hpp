#pragma once

#include "RenderSource.hpp"

namespace doggo::sample::mesh
{
    class MeshSample final : public render::Source
    {
      public:
        MeshSample() noexcept;

        [[nodiscard]] std::span<const render::DrawData> buildDrawData(
            const render::FrameInfo & frameInfo ) noexcept override;
        [[nodiscard]] const render::MeshData & getMeshData() const noexcept override;

      private:
        static constexpr std::size_t sDrawCount = 9;

        render::MeshData                         mMeshData;
        std::array<render::DrawData, sDrawCount> mDraws = {};
    };
} // namespace doggo::sample::mesh
