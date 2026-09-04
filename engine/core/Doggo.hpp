#pragma once

#include <string_view>

namespace doggo
{
    [[nodiscard]] std::string_view getName() noexcept;
    [[nodiscard]] std::string_view getDescription() noexcept;
} // namespace doggo
