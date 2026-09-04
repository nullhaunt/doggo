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

            if ( buttons & HidNpadButton_B )
            {
                result |= doggoMask( input::Button::South );
            }

            if ( buttons & HidNpadButton_A )
            {
                result |= doggoMask( input::Button::East );
            }

            if ( buttons & HidNpadButton_Y )
            {
                result |= doggoMask( input::Button::West );
            }

            if ( buttons & HidNpadButton_X )
            {
                result |= doggoMask( input::Button::North );
            }

            if ( buttons & HidNpadButton_StickL )
            {
                result |= doggoMask( input::Button::LeftStick );
            }

            if ( buttons & HidNpadButton_StickR )
            {
                result |= doggoMask( input::Button::RightStick );
            }

            if ( buttons & HidNpadButton_L )
            {
                result |= doggoMask( input::Button::LeftShoulder );
            }

            if ( buttons & HidNpadButton_R )
            {
                result |= doggoMask( input::Button::RightShoulder );
            }

            if ( buttons & HidNpadButton_ZL )
            {
                result |= doggoMask( input::Button::LeftTrigger );
            }

            if ( buttons & HidNpadButton_ZR )
            {
                result |= doggoMask( input::Button::RightTrigger );
            }

            if ( buttons & HidNpadButton_Plus )
            {
                result |= doggoMask( input::Button::Start );
            }

            if ( buttons & HidNpadButton_Minus )
            {
                result |= doggoMask( input::Button::Back );
            }

            if ( buttons & HidNpadButton_Left )
            {
                result |= doggoMask( input::Button::DPadLeft );
            }

            if ( buttons & HidNpadButton_Up )
            {
                result |= doggoMask( input::Button::DPadUp );
            }

            if ( buttons & HidNpadButton_Right )
            {
                result |= doggoMask( input::Button::DPadRight );
            }

            if ( buttons & HidNpadButton_Down )
            {
                result |= doggoMask( input::Button::DPadDown );
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
