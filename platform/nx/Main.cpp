#include <format>
#include <iostream>

#include <switch.h>

#include "Doggo.hpp"

int main()
{
    consoleInit( nullptr );

    padConfigureInput( 1, HidNpadStyleSet_NpadStandard );

    PadState pad = {};
    padInitializeDefault( &pad );

    std::cout << std::format( "{}\n"
                              "{}\n\n"
                              "Platform: Nintendo Switch\n\n"
                              "DOGGO bootstrap OK.\n\n\n"
                              "Press (+) to exit.",
                              doggo::getName(), doggo::getDescription() );

    while ( appletMainLoop() )
    {
        padUpdate( &pad );

        const u64 buttonsDown = padGetButtonsDown( &pad );
        if ( buttonsDown & HidNpadButton_Plus )
        {
            break;
        }

        consoleUpdate( nullptr );
    }

    consoleExit( nullptr );

    return 0;
}