#include "doggo/platform/nx/nx_Input.hpp"

namespace doggo::platform::nx
{
  void Input::initialize() noexcept
  {
    if ( mIsInitialized )
    {
      return;
    }

    padConfigureInput( 1, HidNpadStyleSet_NpadStandard );
    padInitializeDefault( &mPad );

    mSnapshot      = {};
    mIsInitialized = true;
  }

  const InputSnapshot & Input::update() noexcept
  {
    if ( !mIsInitialized )
    {
      return mSnapshot;
    }

    padUpdate( &mPad );

    const HidAnalogStickState leftStick  = padGetStickPos( &mPad, 0 );
    const HidAnalogStickState rightStick = padGetStickPos( &mPad, 1 );

    mSnapshot = {
        .is_connected = padIsConnected( &mPad ),
        .is_handheld  = padIsHandheld( &mPad ),
        .style_set    = padGetStyleSet( &mPad ),
        .attributes   = padGetAttributes( &mPad ),
        .buttons_held = padGetButtons( &mPad ),
        .buttons_down = padGetButtonsDown( &mPad ),
        .buttons_up   = padGetButtonsUp( &mPad ),
        .left_stick   = { .x = leftStick.x, .y = leftStick.y },
        .right_stick  = { .x = rightStick.x, .y = rightStick.y },
    };

    return mSnapshot;
  }
}  // namespace doggo::platform::nx
