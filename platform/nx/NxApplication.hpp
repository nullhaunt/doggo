#pragma once

#include "Application.hpp"

namespace doggo::platform::nx
{
    class NxApplication final : public ApplicationHost
    {
      public:
        [[nodiscard]] std::string_view getPlatformName() const noexcept override;
        [[nodiscard]] bool             pumpEvents() noexcept override;
    };
} // namespace doggo::platform::nx
