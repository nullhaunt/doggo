#pragma once

#include "Application.hpp"

namespace doggo::platform::windows
{
    class WindowsApplication final : public ApplicationHost
    {
      public:
        [[nodiscard]] std::string_view getPlatformName() const noexcept override;
        [[nodiscard]] bool             pumpEvents() noexcept override;
    };
} // namespace doggo::platform::windows
