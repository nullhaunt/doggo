#pragma once

#include "doggo/doggo_Macro.hpp"
#include "doggo/platform/nx/nx_MonotonicClock.hpp"

#include <switch.h>

#include <array>
#include <cstddef>

namespace doggo::platform::nx
{
  enum class AppletLifecycleEventType : std::uint8_t
  {
    FocusStateChanged,
    OperationModeChanged,
    PerformanceModeChanged,
    ExitRequested,
    Resumed,
  };

  struct AppletLifecycleEvent final
  {
      AppletLifecycleEventType   type = {};
      MonotonicClock::time_point timestamp;
      std::int32_t               detail = 0;
  };

  class AppletLifecycle final
  {
      DOGGO_DISALLOW_COPY( AppletLifecycle );
      DOGGO_DISALLOW_MOVE( AppletLifecycle );

    public:
      AppletLifecycle() noexcept = default;
      ~AppletLifecycle();

      // Configures application-mode focus handling and registers the libnx hook.
      // Returns a libnx Result value. Zero indicates success.
      [[nodiscard]] std::uint32_t initialize() noexcept;
      void                        finalize() noexcept;

      [[nodiscard]] bool          tryPopEvent( AppletLifecycleEvent & event ) noexcept;
      [[nodiscard]] std::uint32_t droppedEventCount() const noexcept;

    private:
      static constexpr std::size_t EventCapacity = 32;

      static void handleAppletHook( AppletHookType hook, void * context );

      void recordCurrentState() noexcept;
      void recordFocusState( AppletFocusState state ) noexcept;
      void recordEvent( AppletLifecycleEventType type, std::int32_t detail = 0 ) noexcept;

      AppletHookCookie                                mHookCookie        = {};
      std::array<AppletLifecycleEvent, EventCapacity> mEvents            = {};
      std::size_t                                     mReadIndex         = 0;
      std::size_t                                     mWriteIndex        = 0;
      std::size_t                                     mEventCount        = 0;
      std::uint32_t                                   mDroppedEventCount = 0;
      std::int32_t                                    mFocusState        = 0;
      bool                                            mIsInitialized     = false;
  };
}  // namespace doggo::platform::nx
