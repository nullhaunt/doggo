#include "Input.hpp"

namespace doggo::input
{
    namespace
    {
        [[nodiscard]] constexpr std::uint64_t getButtonMask( const Button button ) noexcept
        {
            return std::uint64_t{ 1 } << static_cast<std::uint8_t>( button );
        }
    } // namespace

    bool State::isHeld( Button button ) const noexcept
    {
        return ( mHeld & getButtonMask( button ) ) != 0;
    }

    bool State::wasPressed( Button button ) const noexcept
    {
        return ( mPressed & getButtonMask( button ) ) != 0;
    }

    bool State::wasReleased( Button button ) const noexcept
    {
        return ( mReleased & getButtonMask( button ) ) != 0;
    }

    void NullBackend::update() noexcept
    {
        mState.mPressed  = 0;
        mState.mReleased = 0;
    }

    const State & NullBackend::getState() const noexcept
    {
        return mState;
    }
} // namespace doggo::input
