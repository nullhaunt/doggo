#pragma once

namespace doggo::render
{
    class Backend
    {
      public:
        Backend()          = default;
        virtual ~Backend() = default;

        Backend( const Backend & )             = delete;
        Backend & operator=( const Backend & ) = delete;

        Backend( Backend && )             = delete;
        Backend & operator=( Backend && ) = delete;

        [[nodiscard]] virtual bool initialize() noexcept  = 0;
        virtual void               renderFrame() noexcept = 0;
    };

    class NullBackend : public Backend
    {
      public:
        [[nodiscard]] bool initialize() noexcept override;
        void               renderFrame() noexcept override;
    };
} // namespace doggo::render
