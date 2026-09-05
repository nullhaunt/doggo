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
        return mBackend.initialize( mSource.getMeshData() );
    }

    void Renderer::renderFrame( const FrameInfo & frameInfo ) noexcept
    {
        const DrawData drawData = mSource.buildDrawData( frameInfo );
        mBackend.renderFrame( frameInfo, drawData );
    }
} // namespace doggo::render
