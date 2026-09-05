#pragma once

#include "RenderSource.hpp"

namespace doggo::sample::mesh
{
    class MeshSample final : public render::Source
    {
      public:
        MeshSample() noexcept;

        [[nodiscard]] render::DrawData buildDrawData( const render::FrameInfo & frameInfo ) const noexcept override;
        [[nodiscard]] const render::MeshData & getMeshData() const noexcept override;

      private:
        render::MeshData mMeshData;
    };
} // namespace doggo::sample::mesh
