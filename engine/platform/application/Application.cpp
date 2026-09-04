#include "Application.hpp"

#include "Doggo.hpp"
#include "diagnostics/Assert.hpp"
#include "logging/Log.hpp"
#include "time/FrameTimer.hpp"

namespace doggo
{
    int run( platform::ApplicationHost & host, input::Backend & input )
    {
        logging::info( "Application", "{}", getDescription() );
        logging::info( "Application", "Platform: {}", host.getPlatformName() );
        logging::info( "Application", "Bootstrap OK" );

        time::FrameTimer frameTimer;
        while ( host.pumpEvents() )
        {
            input.update();
            frameTimer.tick();

            const input::State & state = input.getState();

            if ( state.wasPressed( input::Button::Start ) )
            {
                logging::info( "Application", "Exit requested" );
                break;
            }

            if ( state.wasPressed( input::Button::Back ) )
            {
                DOGGO_ASSERT( false );
            }
        }

        return 0;
    }
} // namespace doggo
