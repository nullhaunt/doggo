#include "doggo/platform/nx/nx_Application.hpp"

#include <iostream>

#include <switch.h>

namespace doggo::platform::nx
{
  int Application::run()
  {
    if ( !consoleInit( nullptr ) )
    {
      return 1;
    }

    padConfigureInput( 1, HidNpadStyleSet_NpadStandard );

    PadState pad = {};
    padInitializeDefault( &pad );

    std::cout << "Hello, world!\n\n";
    std::cout << "Press (+) to exit.";

    while ( appletMainLoop() )
    {
      padUpdate( &pad );

      const u64 buttonsDown = padGetButtonsDown( &pad );

      if ( ( buttonsDown & HidNpadButton_Plus ) != 0 )
      {
        break;
      }

      consoleUpdate( nullptr );
    }

    consoleExit( nullptr );
    return 0;
  }
}  // namespace doggo::platform::nx