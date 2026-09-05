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

        Source( const Source && )             = delete;
        Source & operator=( const Source && ) = delete;

        [[nodiscard]] virtual DrawData buildDrawData( const FrameInfo & frameInfo ) const noexcept = 0;

        [[nodiscard]] virtual const MeshData & getMeshData() const noexcept = 0;
    };
} // namespace doggo::render
