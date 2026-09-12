#include "doggo/platform/nx/nx_AppletLifecycle.hpp"

namespace doggo::platform::nx
{
  AppletLifecycle::~AppletLifecycle()
  {
    finalize();
  }

  std::uint32_t AppletLifecycle::initialize() noexcept
  {
    if ( mIsInitialized )
    {
      return 0;
    }

    // A regular application reports Background only while it remains runnable.
    // The owner must pause foreground work while focus is elsewhere.
    const Result result = appletSetFocusHandlingMode( AppletFocusHandlingMode_NoSuspend );
    if ( R_FAILED( result ) )
    {
      return result;
    }

    mReadIndex         = 0;
    mWriteIndex        = 0;
    mEventCount        = 0;
    mDroppedEventCount = 0;
    mFocusState        = 0;

    appletHook( &mHookCookie, handleAppletHook, this );
    mIsInitialized = true;
    recordCurrentState();

    return 0;
  }

  void AppletLifecycle::finalize() noexcept
  {
    if ( !mIsInitialized )
    {
      return;
    }

    appletUnhook( &mHookCookie );
    mIsInitialized = false;
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
        lifecycle.recordFocusState( appletGetFocusState() );
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
    recordFocusState( appletGetFocusState() );
    recordEvent( AppletLifecycleEventType::OperationModeChanged,
                 static_cast<std::int32_t>( appletGetOperationMode() ) );
    recordEvent( AppletLifecycleEventType::PerformanceModeChanged, appletGetPerformanceMode() );
  }

  void AppletLifecycle::recordFocusState( const AppletFocusState state ) noexcept
  {
    const auto detail = static_cast<std::int32_t>( state );
    if ( detail == mFocusState )
    {
      return;
    }

    mFocusState = detail;
    recordEvent( AppletLifecycleEventType::FocusStateChanged, detail );
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
