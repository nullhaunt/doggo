#include "doggo/platform/nx/nx_AppletLifecycle.hpp"

namespace doggo::platform::nx
{
  AppletLifecycle::~AppletLifecycle()
  {
    finalize();
  }

  std::uint32_t AppletLifecycle::initialize() noexcept
  {
    if ( mInitialized )
    {
      return 0;
    }

    const Result result = appletSetFocusHandlingMode( AppletFocusHandlingMode_SuspendHomeSleepNotify );
    if ( R_FAILED( result ) )
    {
      return result;
    }

    mReadIndex         = 0;
    mWriteIndex        = 0;
    mEventCount        = 0;
    mDroppedEventCount = 0;

    appletHook( &mHookCookie, handleAppletHook, this );
    mInitialized = true;
    recordCurrentState();

    return 0;
  }

  void AppletLifecycle::finalize() noexcept
  {
    if ( !mInitialized )
    {
      return;
    }

    appletUnhook( &mHookCookie );
    mInitialized = false;
  }

  bool AppletLifecycle::tryPopEvent( AppletLifecycleEvent & event ) noexcept
  {
    if ( mEventCount == 0 )
    {
      return false;
    }

    event      = mEvents[ mReadIndex ];
    mReadIndex = ( mReadIndex + 1 ) % EventCapacity;
    --mEventCount;
    return true;
  }

  std::uint32_t AppletLifecycle::droppedEventCount() const noexcept
  {
    return mDroppedEventCount;
  }

  void AppletLifecycle::handleAppletHook( const AppletHookType hook, void * const context )
  {
    auto & lifecycle = *static_cast<AppletLifecycle *>( context );

    switch ( hook )
    {
      case AppletHookType_OnFocusState:
        lifecycle.recordEvent( AppletLifecycleEventType::FocusStateChanged,
                               static_cast<std::int32_t>( appletGetFocusState() ) );
        break;

      case AppletHookType_OnOperationMode:
        lifecycle.recordEvent( AppletLifecycleEventType::OperationModeChanged,
                               static_cast<std::int32_t>( appletGetOperationMode() ) );
        break;

      case AppletHookType_OnPerformanceMode:
        lifecycle.recordEvent( AppletLifecycleEventType::PerformanceModeChanged, appletGetPerformanceMode() );
        break;

      case AppletHookType_OnExitRequest:
        lifecycle.recordEvent( AppletLifecycleEventType::ExitRequested );
        break;

      case AppletHookType_OnResume:
        lifecycle.recordEvent( AppletLifecycleEventType::Resumed );
        break;

      default:
        break;
    }
  }

  void AppletLifecycle::recordCurrentState() noexcept
  {
    recordEvent( AppletLifecycleEventType::FocusStateChanged, static_cast<std::int32_t>( appletGetFocusState() ) );
    recordEvent( AppletLifecycleEventType::OperationModeChanged,
                 static_cast<std::int32_t>( appletGetOperationMode() ) );
    recordEvent( AppletLifecycleEventType::PerformanceModeChanged, appletGetPerformanceMode() );
  }

  void AppletLifecycle::recordEvent( const AppletLifecycleEventType type, const std::int32_t detail ) noexcept
  {
    if ( mEventCount == EventCapacity )
    {
      mReadIndex = ( mReadIndex + 1 ) % EventCapacity;
      --mEventCount;
      ++mDroppedEventCount;
    }

    mEvents[ mWriteIndex ] = AppletLifecycleEvent{
        .type      = type,
        .timestamp = MonotonicClock::now(),
        .detail    = detail,
    };

    mWriteIndex = ( mWriteIndex + 1 ) % EventCapacity;
    ++mEventCount;
  }
}  // namespace doggo::platform::nx
