#include "RenderBackend.hpp"

namespace doggo::render
{
    bool NullBackend::initialize() noexcept
    {
        return true;
    }

    void NullBackend::renderFrame() noexcept
    {
    }
} // namespace doggo::render
