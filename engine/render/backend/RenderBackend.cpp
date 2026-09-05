#include "RenderBackend.hpp"

namespace doggo::render
{
    bool NullBackend::initialize() noexcept
    {
        return true;
    }

    void NullBackend::renderFrame( const FrameInfo & /*frameInfo*/ ) noexcept
    {
    }
} // namespace doggo::render
