#include "Application.hpp"

#include <format>
#include <iostream>

#include "Doggo.hpp"

namespace doggo
{
    int run( platform::ApplicationHost & host )
    {
        std::cout << std::format( "{}\n"
                                  "{}\n\n"
                                  "Platform: {}\n\n"
                                  "DOGGO bootstrap OK.",
                                  getName(), getDescription(), host.getPlatformName() );

        while ( host.pumpEvents() )
        {
        }

        return 0;
    }
} // namespace doggo