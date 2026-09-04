#include "NxApplication.hpp"

namespace doggo::platform::nx
{
    NxApplication::NxApplication() noexcept
    {
        consoleInit( nullptr );

        padConfigureInput( 1, HidNpadStyleSet_NpadStandard );

        padInitializeDefault( &mPad );
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

        padUpdate( &mPad );

        const u64 buttonsDown = padGetButtonsDown( &mPad );

        if ( buttonsDown & HidNpadButton_Plus )
        {
            return false;
        }

        consoleUpdate( nullptr );
        return true;
    }
} // namespace doggo::platform::nx