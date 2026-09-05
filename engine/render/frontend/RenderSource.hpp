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

        [[nodiscard]] virtual std::span<const DrawRequest> buildDrawRequests(
            const FrameInfo & frameInfo ) noexcept = 0;

        [[nodiscard]] virtual std::span<const MeshData> getMeshes() const noexcept = 0;
    };
} // namespace doggo::render
