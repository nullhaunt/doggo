#include "RenderBackend.hpp"

namespace doggo::render
{
    bool NullBackend::initialize( const MeshData & /*meshData*/ ) noexcept
    {
        return true;
    }

    void NullBackend::renderFrame( const FrameInfo & /*frameInfo*/, std::span<const DrawData> /*draws*/ ) noexcept
    {
    }
} // namespace doggo::render
