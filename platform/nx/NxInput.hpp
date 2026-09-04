#pragma once

#include <switch.h>

#include "Input.hpp"

namespace doggo::platform::nx
{
    class NxInput final : public input::Backend
    {
      public:
        NxInput() noexcept;

        void update() noexcept override;

        [[nodiscard]] const input::State & getState() const noexcept override;

      private:
        PadState     mPad = {};
        input::State mState;
    };
} // namespace doggo::platform::nx
