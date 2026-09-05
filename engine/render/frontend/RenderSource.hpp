#pragma once

#include "RenderBackend.hpp"

namespace doggo::render
{
    class Source
    {
      public:
        Source()          = default;
        virtual ~Source() = default;

        Source( const Source & )             = delete;
        Source & operator=( const Source & ) = delete;

        Source( Source && )             = delete;
        Source & operator=( Source && ) = delete;

        [[nodiscard]] virtual std::span<const DrawData> buildDrawData( const FrameInfo & frameInfo ) noexcept = 0;

        [[nodiscard]] virtual const MeshData & getMeshData() const noexcept = 0;
    };
} // namespace doggo::render
