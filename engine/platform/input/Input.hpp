#pragma once

#include <cstdint>

namespace doggo::input
{
    enum class Button : std::uint8_t
    {
        South,
        East,
        West,
        North,

        LeftStick,
        RightStick,

        LeftShoulder,
        RightShoulder,

        LeftTrigger,
        RightTrigger,

        Start,
        Back,

        DPadLeft,
        DPadUp,
        DPadRight,
        DPadDown,
    };

    struct StickState
    {
        float mX = 0.0f;
        float mY = 0.0f;
    };

    struct State
    {
        bool mIsConnected = false;

        std::uint64_t mHeld     = 0;
        std::uint64_t mPressed  = 0;
        std::uint64_t mReleased = 0;

        StickState mLeftStick;
        StickState mRightStick;

        float mLeftTrigger  = 0.0f;
        float mRightTrigger = 0.0f;

        [[nodiscard]] bool isHeld( Button button ) const noexcept;
        [[nodiscard]] bool wasPressed( Button button ) const noexcept;
        [[nodiscard]] bool wasReleased( Button button ) const noexcept;
    };

    class Backend
    {
      public:
        Backend()          = default;
        virtual ~Backend() = default;

        Backend( const Backend & )             = delete;
        Backend & operator=( const Backend & ) = delete;

        Backend( Backend && )             = delete;
        Backend & operator=( Backend && ) = delete;

        virtual void update() noexcept = 0;

        [[nodiscard]] virtual const State & getState() const noexcept = 0;
    };

    class NullBackend final : public Backend
    {
      public:
        void update() noexcept override;

        [[nodiscard]] const State & getState() const noexcept override;

      private:
        State mState;
    };
} // namespace doggo::input
