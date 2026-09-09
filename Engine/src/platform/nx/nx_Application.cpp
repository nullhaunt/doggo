#include "doggo/platform/nx/nx_Application.hpp"

#include "doggo/log/Log.hpp"
#include "doggo/platform/nx/nx_AppletLifecycle.hpp"
#include "doggo/platform/nx/nx_Input.hpp"
#include "doggo/platform/nx/nx_Memory.hpp"
#include "doggo/platform/nx/nx_MonotonicClock.hpp"
#include "nx_LogSinks.hpp"

#include <switch.h>

#include <chrono>
#include <cstdlib>
#include <format>
#include <limits>
#include <string>
#include <string_view>

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

  [[nodiscard]] std::chrono::nanoseconds
  elapsedSince( const doggo::platform::nx::MonotonicClock::time_point timestamp,
                const doggo::platform::nx::MonotonicClock::time_point startedAt ) noexcept
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>( timestamp - startedAt );
  }

  void writeLog( doggo::log::Logger &                                  logger,
                 const doggo::log::Level                               level,
                 const std::string_view                                category,
                 const std::string_view                                message,
                 const doggo::platform::nx::MonotonicClock::time_point timestamp,
                 const doggo::platform::nx::MonotonicClock::time_point startedAt ) noexcept
  {
    logger.write( level, category, message, elapsedSince( timestamp, startedAt ) );
  }

  [[nodiscard]] bool isStickLive( const doggo::platform::nx::AnalogStickPosition & stick ) noexcept
  {
    return stick.x < -StickDirectionThreshold ||
           stick.x > StickDirectionThreshold ||
           stick.y < -StickDirectionThreshold ||
           stick.y > StickDirectionThreshold;
  }

  void logStickChange( doggo::log::Logger &                                  logger,
                       const char * const                                    stickName,
                       const doggo::platform::nx::AnalogStickPosition &      position,
                       const bool                                            isLive,
                       const doggo::platform::nx::MonotonicClock::time_point timestamp,
                       const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    writeLog( logger,
              doggo::log::Level::Info,
              "Input",
              std::format( "{} stick {} ({}, {})", stickName, isLive ? "active" : "centered", position.x, position.y ),
              timestamp,
              startedAt );
  }

  void logInputChanges( doggo::log::Logger &                                  logger,
                        const doggo::platform::nx::InputSnapshot &            input,
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
      if ( input.is_connected )
      {
        const bool         wasConnected = previous.has_sample && previous.is_connected;
        const char * const source       = input.is_handheld ? "Handheld" : "External";
        writeLog( logger,
                  doggo::log::Level::Info,
                  "Input",
                  std::format( "Controller {} ({}, style 0x{:08X}, attributes 0x{:08X})",
                               wasConnected ? "configuration changed" : "connected",
                               source,
                               input.style_set,
                               input.attributes ),
                  timestamp,
                  startedAt );
      }
      else
      {
        writeLog( logger, doggo::log::Level::Warning, "Input", "Controller disconnected", timestamp, startedAt );
      }

      previous.is_left_stick_live  = false;
      previous.is_right_stick_live = false;
    }

    const std::uint64_t buttonsHeld = input.buttons_held & ~StickPseudoButtonMask;
    const std::uint64_t buttonsDown = input.buttons_down & ~StickPseudoButtonMask;
    const std::uint64_t buttonsUp   = input.buttons_up & ~StickPseudoButtonMask;
    if ( buttonsDown != 0 || buttonsUp != 0 )
    {
      writeLog( logger,
                doggo::log::Level::Info,
                "Input",
                std::format( "Buttons held 0x{:09X}, down 0x{:09X}, up 0x{:09X}", buttonsHeld, buttonsDown, buttonsUp ),
                timestamp,
                startedAt );
    }

    if ( input.is_connected )
    {
      const bool isLeftStickLive = isStickLive( input.left_stick );
      if ( isLeftStickLive != previous.is_left_stick_live )
      {
        logStickChange( logger, "Left", input.left_stick, isLeftStickLive, timestamp, startedAt );
      }

      const bool isRightStickLive = isStickLive( input.right_stick );
      if ( isRightStickLive != previous.is_right_stick_live )
      {
        logStickChange( logger, "Right", input.right_stick, isRightStickLive, timestamp, startedAt );
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

  void logMemoryReport( doggo::log::Logger &                                  logger,
                        const doggo::platform::nx::MemoryReport &             report,
                        const doggo::platform::nx::MonotonicClock::time_point timestamp,
                        const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    writeLog( logger,
              doggo::log::Level::Info,
              "Memory",
              std::format( "Process total: {:.2f} MiB",
                           static_cast<double>( report.process_total_bytes ) / BytesPerMebibyte ),
              timestamp,
              startedAt );
    writeLog(
        logger,
        doggo::log::Level::Info,
        "Memory",
        std::format( "Process used: {:.2f} MiB", static_cast<double>( report.process_used_bytes ) / BytesPerMebibyte ),
        timestamp,
        startedAt );
    writeLog(
        logger,
        doggo::log::Level::Info,
        "Memory",
        std::format( "Process free: {:.2f} MiB", static_cast<double>( report.process_free_bytes ) / BytesPerMebibyte ),
        timestamp,
        startedAt );
    writeLog(
        logger,
        doggo::log::Level::Info,
        "Memory",
        std::format( "Heap region: {:.2f} MiB", static_cast<double>( report.heap_region_bytes ) / BytesPerMebibyte ),
        timestamp,
        startedAt );
  }

  [[nodiscard]] std::string formatResult( const std::uint32_t result )
  {
    return std::format( "0x{:08X}", result );
  }

  void logLifecycleEvent( doggo::log::Logger &                                  logger,
                          const doggo::platform::nx::AppletLifecycleEvent &     event,
                          const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    using EventType = doggo::platform::nx::AppletLifecycleEventType;
    std::string message;
    switch ( event.type )
    {
      case EventType::FocusStateChanged:
        message = std::format( "Focus: {}", getFocusStateName( event.detail ) );
        break;

      case EventType::OperationModeChanged:
        message = std::format( "Operation mode: {}", getOperationModeName( event.detail ) );
        break;

      case EventType::PerformanceModeChanged:
        message = std::format( "Performance mode: {}", getPerformanceModeName( event.detail ) );
        break;

      case EventType::ExitRequested:
        message = "Exit requested by applet service";
        break;

      case EventType::Resumed:
        message = "Application resumed";
        break;
    }

    writeLog( logger, doggo::log::Level::Info, "Lifecycle", message, event.timestamp, startedAt );
  }
}  // namespace

namespace doggo::platform::nx
{
  int Application::run()
  {
    if ( !consoleInit( nullptr ) )
    {
      return EXIT_FAILURE;
    }

    const MonotonicClock::time_point startedAt = MonotonicClock::now();

    log::Logger    logger;
    ConsoleLogSink consoleSink;
    NxlinkLogSink  nxlinkSink;
    if ( !logger.attach( consoleSink ) )
    {
      consoleExit( nullptr );
      return EXIT_FAILURE;
    }

    const bool isNxlinkConnected = nxlinkSink.initialize() && logger.attach( nxlinkSink );
    if ( isNxlinkConnected )
    {
      writeLog( logger,
                log::Level::Info,
                "Logging",
                "PC log stream connected through nxlink",
                MonotonicClock::now(),
                startedAt );
    }
    else
    {
      nxlinkSink.finalize();
      writeLog( logger,
                log::Level::Warning,
                "Logging",
                "PC log stream unavailable; launch with nxlink -s to attach it",
                MonotonicClock::now(),
                startedAt );
    }

    AppletLifecycle     lifecycle;
    const std::uint32_t lifecycleResult = lifecycle.initialize();

    Input input;
    input.initialize();

    writeLog( logger, log::Level::Info, "Startup", "DOGGO Gate 0 - Platform Proof", startedAt, startedAt );
    writeLog( logger,
              log::Level::Info,
              "Timing",
              std::format( "Counter frequency: {} Hz", MonotonicClock::frequency() ),
              MonotonicClock::now(),
              startedAt );

    int exitCode = EXIT_SUCCESS;

    if ( R_FAILED( lifecycleResult ) )
    {
      writeLog( logger,
                log::Level::Error,
                "Lifecycle",
                std::format( "Hook initialization failed: {}", formatResult( lifecycleResult ) ),
                MonotonicClock::now(),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    MemoryReport        memoryReport;
    const std::uint32_t memoryResult = queryMemoryReport( memoryReport );
    if ( R_SUCCEEDED( memoryResult ) )
    {
      logMemoryReport( logger, memoryReport, MonotonicClock::now(), startedAt );
    }
    else
    {
      writeLog( logger,
                log::Level::Error,
                "Memory",
                std::format( "Query failed: {}", formatResult( memoryResult ) ),
                MonotonicClock::now(),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    writeLog( logger,
              log::Level::Info,
              "Input",
              "Exercise connection, buttons, and both sticks; press (+) to exit",
              MonotonicClock::now(),
              startedAt );

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

        logLifecycleEvent( logger, lifecycleEvent, startedAt );
      }

      const MonotonicClock::time_point currentTime = MonotonicClock::now();
      if ( currentTime < previousTime )
      {
        writeLog( logger, log::Level::Error, "Timing", "Monotonic clock moved backwards", currentTime, startedAt );
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
          writeLog( logger,
                    log::Level::Error,
                    "Lifecycle",
                    std::format( "Wait failed: {}", formatResult( waitResult ) ),
                    currentTime,
                    startedAt );
          exitCode = EXIT_FAILURE;
          break;
        }

        continue;
      }

      const InputSnapshot & inputSnapshot = input.update();
      logInputChanges( logger, inputSnapshot, inputTelemetry, currentTime, startedAt );

      if ( ( inputSnapshot.buttons_down & HidNpadButton_Plus ) != 0 )
      {
        writeLog( logger, log::Level::Info, "Input", "Exit requested by controller", currentTime, startedAt );
        break;
      }

      consoleUpdate( nullptr );
    }

    if ( lifecycle.droppedEventCount() != 0 )
    {
      writeLog( logger,
                log::Level::Warning,
                "Lifecycle",
                std::format( "Dropped {} lifecycle events", lifecycle.droppedEventCount() ),
                MonotonicClock::now(),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    writeLog( logger,
              log::Level::Info,
              "Shutdown",
              std::format( "Application exiting with code {}", exitCode ),
              MonotonicClock::now(),
              startedAt );

    lifecycle.finalize();
    logger.flush();
    consoleUpdate( nullptr );
    logger.detach( nxlinkSink );
    nxlinkSink.finalize();
    logger.detach( consoleSink );
    consoleExit( nullptr );
    return exitCode;
  }
}  // namespace doggo::platform::nx
