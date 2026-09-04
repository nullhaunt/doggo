#pragma once

#include <switch.h>

#include "Application.hpp"

namespace doggo::platform::nx
{
    class NxApplication final : public ApplicationHost
    {
      public:
        NxApplication() noexcept;
        ~NxApplication() override;

        [[nodiscard]] std::string_view getPlatformName() const noexcept override;
        [[nodiscard]] bool             pumpEvents() noexcept override;

      private:
        PadState mPad = {};
    };
} // namespace doggo::platform::nx