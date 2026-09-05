#include "RenderBackend.hpp"

namespace doggo::render
{
    bool NullBackend::initialize() noexcept
    {
        return true;
    }

    void NullBackend::renderFrame( const FrameInfo & /*frameInfo*/, std::span<const DrawPacket> /*draws*/ ) noexcept
    {
    }

    MeshHandle NullBackend::createMesh( const MeshData & /*meshData*/ ) noexcept
    {
        return {};
    }
} // namespace doggo::render
