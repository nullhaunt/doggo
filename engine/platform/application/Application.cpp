#include "Application.hpp"

#include <format>
#include <iostream>

#include "Doggo.hpp"
#include "time/FrameTimer.hpp"

namespace doggo
{
    int run( platform::ApplicationHost & host )
    {
        std::cout << std::format( "{}\n"
                                  "{}\n\n"
                                  "Platform: {}\n\n"
                                  "DOGGO bootstrap OK.",
                                  getName(), getDescription(), host.getPlatformName() );

        time::FrameTimer frameTimer;
        while ( host.pumpEvents() )
        {
            frameTimer.tick();
        }

        return 0;
    }
} // namespace doggo