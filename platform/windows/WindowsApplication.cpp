#include "WindowsApplication.hpp"

namespace doggo::platform::windows
{
    std::string_view WindowsApplication::getPlatformName() const noexcept
    {
        return "Windows";
    }

    bool WindowsApplication::pumpEvents() noexcept
    {
        return false;
    }
} // namespace doggo::platform::windows
