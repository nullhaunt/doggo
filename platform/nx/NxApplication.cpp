#include "NxApplication.hpp"

#include <switch.h>

namespace doggo::platform::nx
{
    NxApplication::NxApplication() noexcept
    {
        consoleInit( nullptr );
    }

    NxApplication::~NxApplication()
    {
        consoleExit( nullptr );
    }

    std::string_view NxApplication::getPlatformName() const noexcept
    {
        return "Nintendo Switch";
    }

    bool NxApplication::pumpEvents() noexcept
    {
        if ( !appletMainLoop() )
        {
            return false;
        }

        consoleUpdate( nullptr );
        return true;
    }
} // namespace doggo::platform::nx
