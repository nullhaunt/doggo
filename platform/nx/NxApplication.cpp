#include "NxApplication.hpp"

#include <switch.h>

namespace doggo::platform::nx
{
    std::string_view NxApplication::getPlatformName() const noexcept
    {
        return "Nintendo Switch";
    }

    bool NxApplication::pumpEvents() noexcept
    {
        return appletMainLoop();
    }
} // namespace doggo::platform::nx
