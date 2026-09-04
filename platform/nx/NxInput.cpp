#include "NxInput.hpp"

#include <algorithm>

namespace doggo::platform::nx
{
    namespace
    {
        [[nodiscard]] constexpr std::uint64_t doggoMask( const input::Button button ) noexcept
        {
            return std::uint64_t{ 1 } << static_cast<std::uint8_t>( button );
        }

        [[nodiscard]] std::uint64_t translateButtons( const u64 buttons ) noexcept
        {
            std::uint64_t result = 0;

            switch ( buttons )
            {
                case HidNpadButton_B:
                    result |= doggoMask( input::Button::South );
                    break;

                case HidNpadButton_A:
                    result |= doggoMask( input::Button::East );
                    break;

                case HidNpadButton_Y:
                    result |= doggoMask( input::Button::West );
                    break;

                case HidNpadButton_X:
                    result |= doggoMask( input::Button::North );
                    break;

                case HidNpadButton_StickL:
                    result |= doggoMask( input::Button::LeftStick );
                    break;

                case HidNpadButton_StickR:
                    result |= doggoMask( input::Button::RightStick );
                    break;

                case HidNpadButton_L:
                    result |= doggoMask( input::Button::LeftShoulder );
                    break;

                case HidNpadButton_R:
                    result |= doggoMask( input::Button::RightShoulder );
                    break;

                case HidNpadButton_ZL:
                    result |= doggoMask( input::Button::LeftTrigger );
                    break;

                case HidNpadButton_ZR:
                    result |= doggoMask( input::Button::RightTrigger );
                    break;

                case HidNpadButton_Plus:
                    result |= doggoMask( input::Button::Start );
                    break;

                case HidNpadButton_Minus:
                    result |= doggoMask( input::Button::Back );
                    break;

                case HidNpadButton_Left:
                    result |= doggoMask( input::Button::DPadLeft );
                    break;

                case HidNpadButton_Up:
                    result |= doggoMask( input::Button::DPadUp );
                    break;

                case HidNpadButton_Right:
                    result |= doggoMask( input::Button::DPadRight );
                    break;

                case HidNpadButton_Down:
                    result |= doggoMask( input::Button::DPadDown );
                    break;

                default:
                    result = 0;
            }

            return result;
        }

        [[nodiscard]] float normalizeStickAxis( const std::int32_t value ) noexcept
        {
            const float normalized = static_cast<float>( value ) / static_cast<float>( JOYSTICK_MAX );
            return std::ranges::clamp( normalized, -1.0f, 1.0f );
        }

        [[nodiscard]] input::StickState convertStick( const HidAnalogStickState stick ) noexcept
        {
            return { .mX = normalizeStickAxis( stick.x ), .mY = normalizeStickAxis( stick.y ) };
        }
    } // namespace

    NxInput::NxInput() noexcept
    {
        padConfigureInput( 1, HidNpadStyleSet_NpadStandard );
        padInitializeDefault( &mPad );
    }

    void NxInput::update() noexcept
    {
        padUpdate( &mPad );

        const u64                 held       = padGetButtons( &mPad );
        const u64                 pressed    = padGetButtonsDown( &mPad );
        const u64                 released   = padGetButtonsUp( &mPad );
        const HidAnalogStickState leftStick  = padGetStickPos( &mPad, 0 );
        const HidAnalogStickState rightStick = padGetStickPos( &mPad, 1 );

        mState.mIsConnected  = padIsConnected( &mPad );
        mState.mHeld         = translateButtons( held );
        mState.mPressed      = translateButtons( pressed );
        mState.mReleased     = translateButtons( released );
        mState.mLeftStick    = convertStick( leftStick );
        mState.mRightStick   = convertStick( rightStick );
        mState.mLeftTrigger  = ( held & HidNpadButton_ZL ) != 0 ? 1.0f : 0.0f;
        mState.mRightTrigger = ( held & HidNpadButton_ZR ) != 0 ? 1.0f : 0.0f;
    }

    const input::State & NxInput::getState() const noexcept
    {
        return mState;
    }
} // namespace doggo::platform::nx
