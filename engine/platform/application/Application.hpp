#pragma once

#include <string_view>

#include "Input.hpp"

namespace doggo::platform
{
    class ApplicationHost
    {
      public:
        ApplicationHost()          = default;
        virtual ~ApplicationHost() = default;

        ApplicationHost( const ApplicationHost & )             = delete;
        ApplicationHost & operator=( const ApplicationHost & ) = delete;

        ApplicationHost( ApplicationHost && )             = delete;
        ApplicationHost & operator=( ApplicationHost && ) = delete;

        [[nodiscard]] virtual std::string_view getPlatformName() const noexcept = 0;
        [[nodiscard]] virtual bool             pumpEvents() noexcept            = 0;
    };
} // namespace doggo::platform

namespace doggo
{
    [[nodiscard]] int run( platform::ApplicationHost & host, input::Backend & input );
}
