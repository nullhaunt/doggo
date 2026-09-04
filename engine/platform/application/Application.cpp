#include "Application.hpp"

#include "Doggo.hpp"
#include "logging/Log.hpp"
#include "time/FrameTimer.hpp"

namespace doggo
{
    int run( platform::ApplicationHost & host )
    {
        logging::info( "Application", "{}", getDescription() );
        logging::info( "Application", "Platform: {}", host.getPlatformName() );
        logging::info( "Application", "Bootstrap OK" );

        time::FrameTimer frameTimer;
        while ( host.pumpEvents() )
        {
            frameTimer.tick();
        }

        return 0;
    }
} // namespace doggo
