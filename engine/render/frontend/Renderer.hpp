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
        Backend & mBackend;
        Source &  mSource;
    };
} // namespace doggo::render
