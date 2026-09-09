#include "doggo/platform/nx/nx_Application.hpp"

#include "doggo/platform/nx/nx_AppletLifecycle.hpp"
#include "doggo/platform/nx/nx_Input.hpp"
#include "doggo/platform/nx/nx_Memory.hpp"
#include "doggo/platform/nx/nx_MonotonicClock.hpp"

#include <switch.h>

#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <limits>

namespace
{
  constexpr double       BytesPerMebibyte        = 1024.0 * 1024.0;
  constexpr std::int32_t StickDirectionThreshold = JOYSTICK_MAX / 4;

  constexpr std::uint64_t StickPseudoButtonMask = HidNpadButton_StickLLeft |
                                                  HidNpadButton_StickLUp |
                                                  HidNpadButton_StickLRight |
                                                  HidNpadButton_StickLDown |
                                                  HidNpadButton_StickRLeft |
                                                  HidNpadButton_StickRUp |
                                                  HidNpadButton_StickRRight |
                                                  HidNpadButton_StickRDown;

  struct InputTelemetryState final
  {
      bool          has_sample          = false;
      bool          is_connected        = false;
      bool          is_handheld         = false;
      bool          is_left_stick_live  = false;
      bool          is_right_stick_live = false;
      std::uint32_t style_set           = 0;
      std::uint32_t attributes          = 0;
  };

  [[nodiscard]] bool isStickLive( const doggo::platform::nx::AnalogStickPosition & stick ) noexcept
  {
    return stick.x < -StickDirectionThreshold ||
           stick.x > StickDirectionThreshold ||
           stick.y < -StickDirectionThreshold ||
           stick.y > StickDirectionThreshold;
  }

  void printTimestamp( const doggo::platform::nx::MonotonicClock::time_point timestamp,
                       const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    const auto elapsed = std::chrono::duration<double, std::milli>{ timestamp - startedAt }.count();
    std::cout << std::format( "[+{:.3f} ms] ", elapsed );
  }

  void printStickChange( const char * const                                    stickName,
                         const doggo::platform::nx::AnalogStickPosition &      position,
                         const bool                                            isLive,
                         const doggo::platform::nx::MonotonicClock::time_point timestamp,
                         const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    printTimestamp( timestamp, startedAt );
    std::cout << std::format( "Input: {} stick {} ({}, {})\n", stickName, isLive ? "active" : "centered", position.x,
                              position.y );
  }

  void printInputChanges( const doggo::platform::nx::InputSnapshot &            input,
                          InputTelemetryState &                                 previous,
                          const doggo::platform::nx::MonotonicClock::time_point timestamp,
                          const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    const bool isSourceChanged = !previous.has_sample ||
                                 input.is_connected != previous.is_connected ||
                                 input.is_handheld != previous.is_handheld ||
                                 input.style_set != previous.style_set ||
                                 input.attributes != previous.attributes;

    if ( isSourceChanged )
    {
      printTimestamp( timestamp, startedAt );
      if ( input.is_connected )
      {
        const bool         wasConnected = previous.has_sample && previous.is_connected;
        const char * const source       = input.is_handheld ? "Handheld" : "External";
        std::cout << std::format( "Input: Controller {} ({}, style 0x{:08X}, attributes 0x{:08X})\n",
                                  wasConnected ? "configuration changed" : "connected",
                                  source,
                                  input.style_set,
                                  input.attributes );
      }
      else
      {
        std::cout << "Input: Controller disconnected\n";
      }

      previous.is_left_stick_live  = false;
      previous.is_right_stick_live = false;
    }

    const std::uint64_t buttonsHeld = input.buttons_held & ~StickPseudoButtonMask;
    const std::uint64_t buttonsDown = input.buttons_down & ~StickPseudoButtonMask;
    const std::uint64_t buttonsUp   = input.buttons_up & ~StickPseudoButtonMask;
    if ( buttonsDown != 0 || buttonsUp != 0 )
    {
      printTimestamp( timestamp, startedAt );
      std::cout << std::format( "Input: Buttons held 0x{:09X}, down 0x{:09X}, up 0x{:09X}\n",
                                buttonsHeld,
                                buttonsDown,
                                buttonsUp );
    }

    if ( input.is_connected )
    {
      const bool isLeftStickLive = isStickLive( input.left_stick );
      if ( isLeftStickLive != previous.is_left_stick_live )
      {
        printStickChange( "Left", input.left_stick, isLeftStickLive, timestamp, startedAt );
      }

      const bool isRightStickLive = isStickLive( input.right_stick );
      if ( isRightStickLive != previous.is_right_stick_live )
      {
        printStickChange( "Right", input.right_stick, isRightStickLive, timestamp, startedAt );
      }

      previous.is_left_stick_live  = isLeftStickLive;
      previous.is_right_stick_live = isRightStickLive;
    }

    previous.has_sample   = true;
    previous.is_connected = input.is_connected;
    previous.is_handheld  = input.is_handheld;
    previous.style_set    = input.style_set;
    previous.attributes   = input.attributes;
  }

  [[nodiscard]] const char * getFocusStateName( const std::int32_t state ) noexcept
  {
    switch ( static_cast<AppletFocusState>( state ) )
    {
      case AppletFocusState_InFocus:
        return "In Focus";

      case AppletFocusState_OutOfFocus:
        return "Out of Focus";

      case AppletFocusState_Background:
        return "Background";

      default:
        return "Unknown";
    }
  }

  [[nodiscard]] const char * getOperationModeName( const std::int32_t mode ) noexcept
  {
    switch ( static_cast<AppletOperationMode>( mode ) )
    {
      case AppletOperationMode_Handheld:
        return "Handheld";

      case AppletOperationMode_Console:
        return "Docked";

      default:
        return "Unknown";
    }
  }

  [[nodiscard]] const char * getPerformanceModeName( const std::int32_t mode ) noexcept
  {
    switch ( static_cast<ApmPerformanceMode>( mode ) )
    {
      case ApmPerformanceMode_Normal:
        return "Normal";

      case ApmPerformanceMode_Boost:
        return "Boost";

      default:
        return "Unknown";
    }
  }

  void printMemoryReport( const doggo::platform::nx::MemoryReport & report )
  {
    std::cout << "Memory\n";
    std::cout << std::format( "\tProcess total : {:>7.2f}\tMiB\n",
                              static_cast<double>( report.process_total_bytes ) / BytesPerMebibyte );
    std::cout << std::format( "\tProcess used  : {:>7.2f}\tMiB\n",
                              static_cast<double>( report.process_used_bytes ) / BytesPerMebibyte );
    std::cout << std::format( "\tProcess free  : {:>7.2f}\tMiB\n",
                              static_cast<double>( report.process_free_bytes ) / BytesPerMebibyte );
    std::cout << std::format( "\tHeap region   : {:>7.2f}\tMiB\n\n",
                              static_cast<double>( report.heap_region_bytes ) / BytesPerMebibyte );
  }

  void printResult( const std::uint32_t result )
  {
    std::cout << std::format( "0x{:08X}", result );
  }

  void printLifecycleEvent( const doggo::platform::nx::AppletLifecycleEvent &     event,
                            const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    printTimestamp( event.timestamp, startedAt );

    using EventType = doggo::platform::nx::AppletLifecycleEventType;
    switch ( event.type )
    {
      case EventType::FocusStateChanged:
        std::cout << std::format( "Focus: {}", getFocusStateName( event.detail ) );
        break;

      case EventType::OperationModeChanged:
        std::cout << std::format( "Operation mode: {}", getOperationModeName( event.detail ) );
        break;

      case EventType::PerformanceModeChanged:
        std::cout << std::format( "Performance mode: {}", getPerformanceModeName( event.detail ) );
        break;

      case EventType::ExitRequested:
        std::cout << "Exit requested by applet service";
        break;

      case EventType::Resumed:
        std::cout << "Application resumed";
        break;
    }

    std::cout << '\n';
  }
}  // namespace

namespace doggo::platform::nx
{
  int Application::run()
  {
    if ( !consoleInit( nullptr ) )
    {
      return 1;
    }

    const MonotonicClock::time_point startedAt = MonotonicClock::now();

    AppletLifecycle     lifecycle;
    const std::uint32_t lifecycleResult = lifecycle.initialize();

    Input input;
    input.initialize();

    std::cout << "DOGGO Gate 0 - Platform Proof\n";
    std::cout << "=============================\n\n";
    std::cout << "Timing\n";
    std::cout << std::format( "\tCounter frequency: {} Hz\n\n", MonotonicClock::frequency() );

    int exitCode = EXIT_SUCCESS;

    if ( R_FAILED( lifecycleResult ) )
    {
      std::cout << std::format( "Lifecycle hook initialization failed: " );
      printResult( lifecycleResult );
      std::cout << "\n\n";
      exitCode = EXIT_FAILURE;
    }

    MemoryReport        memoryReport;
    const std::uint32_t memoryResult = queryMemoryReport( memoryReport );
    if ( R_SUCCEEDED( memoryResult ) )
    {
      printMemoryReport( memoryReport );
    }
    else
    {
      std::cout << "Memory query failed: ";
      printResult( memoryResult );
      std::cout << "\n\n";
      exitCode = EXIT_FAILURE;
    }

    std::cout << "Input\n";
    std::cout << "\tExercise connection, buttons, and both sticks.\n\n";
    std::cout << "Press (+) to exit.\n\n";
    std::cout << "Events\n";

    MonotonicClock::time_point previousTime = startedAt;
    bool                       isRunning    = true;
    AppletFocusState           focusState   = AppletFocusState_InFocus;
    InputTelemetryState        inputTelemetry;

    while ( isRunning )
    {
      isRunning = appletMainLoop();

      AppletLifecycleEvent lifecycleEvent;
      while ( lifecycle.tryPopEvent( lifecycleEvent ) )
      {
        if ( lifecycleEvent.type == AppletLifecycleEventType::FocusStateChanged )
        {
          focusState = static_cast<AppletFocusState>( lifecycleEvent.detail );
        }

        printLifecycleEvent( lifecycleEvent, startedAt );
      }

      const MonotonicClock::time_point currentTime = MonotonicClock::now();
      if ( currentTime < previousTime )
      {
        std::cout << "ERROR: monotonic clock moved backwards.\n";
        exitCode = EXIT_FAILURE;
        break;
      }
      previousTime = currentTime;

      if ( !isRunning )
      {
        break;
      }

      if ( focusState != AppletFocusState_InFocus )
      {
        // NoSuspend keeps the process alive so it can observe the transition.
        // Block foreground work until the next lifecycle message instead of
        // spinning behind HOME or a foreground library applet.
        const Result waitResult = eventWait( appletGetMessageEvent(), std::numeric_limits<std::uint64_t>::max() );
        if ( R_FAILED( waitResult ) )
        {
          std::cout << "Lifecycle wait failed: ";
          printResult( waitResult );
          std::cout << '\n';
          exitCode = EXIT_FAILURE;
          break;
        }

        continue;
      }

      const InputSnapshot & inputSnapshot = input.update();
      printInputChanges( inputSnapshot, inputTelemetry, currentTime, startedAt );

      if ( ( inputSnapshot.buttons_down & HidNpadButton_Plus ) != 0 )
      {
        printTimestamp( currentTime, startedAt );
        std::cout << "Exit requested by controller\n";
        break;
      }

      consoleUpdate( nullptr );
    }

    if ( lifecycle.droppedEventCount() != 0 )
    {
      std::cout << std::format( "WARNING: dropped {} lifecycle events.\n", lifecycle.droppedEventCount() );
      exitCode = EXIT_FAILURE;
    }

    consoleUpdate( nullptr );
    lifecycle.finalize();
    consoleExit( nullptr );
    return exitCode;
  }
}  // namespace doggo::platform::nx
