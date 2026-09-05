#include "Renderer.hpp"

namespace doggo::render
{
    Renderer::Renderer( Backend & backend, Source & source ) noexcept
        : mBackend( backend )
        , mSource( source )
    {
    }

    bool Renderer::initialize() noexcept
    {
        if ( !mBackend.initialize() )
        {
            return false;
        }

        const std::span<const MeshData> meshes = mSource.getMeshes();

        if ( meshes.size() > sMeshCapacity )
        {
            return false;
        }

        for ( const MeshData & mesh : meshes )
        {
            const MeshHandle handle = mBackend.createMesh( mesh );

            if ( !handle.isValid() )
            {
                return false;
            }

            mMeshes[ mMeshCount++ ] = handle;
        }

        return true;
    }

    void Renderer::renderFrame( const FrameInfo & frameInfo ) noexcept
    {
        const std::span<const DrawRequest> requests = mSource.buildDrawRequests( frameInfo );

        std::size_t packetCount = 0;

        for ( const DrawRequest & request : requests )
        {
            if ( packetCount >= sDrawCapacity )
            {
                break;
            }

            if ( request.mMeshId >= mMeshCount )
            {
                continue;
            }

            mDrawPackets[ packetCount++ ] =
                DrawPacket{ .mMesh = mMeshes[ request.mMeshId ], .mDrawData = request.mDrawData };
        }

        mBackend.renderFrame( frameInfo, std::span<const DrawPacket>{ mDrawPackets.data(), packetCount } );
    }
} // namespace doggo::render
