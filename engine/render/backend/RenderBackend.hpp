#pragma once

#include <chrono>

namespace doggo::render
{
    struct FrameInfo
    {
        std::uint64_t            mFrameIndex  = 0;
        std::chrono::nanoseconds mDeltaTime   = {};
        std::chrono::nanoseconds mElapsedTime = {};
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

        [[nodiscard]] virtual bool initialize() noexcept                               = 0;
        virtual void               renderFrame( const FrameInfo & frameInfo ) noexcept = 0;
    };

    class NullBackend : public Backend
    {
      public:
        [[nodiscard]] bool initialize() noexcept override;
        void               renderFrame( const FrameInfo & frameInfo ) noexcept override;
    };
} // namespace doggo::render
