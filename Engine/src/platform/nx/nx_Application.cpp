#include "doggo/platform/nx/nx_Application.hpp"

#include "doggo/log/log_Log.hpp"
#include "doggo/platform/nx/nx_AppletLifecycle.hpp"
#include "doggo/platform/nx/nx_AudrenTone.hpp"
#include "doggo/platform/nx/nx_Input.hpp"
#include "doggo/platform/nx/nx_LogSinks.hpp"
#include "doggo/platform/nx/nx_Memory.hpp"
#include "doggo/platform/nx/nx_MonotonicClock.hpp"

#include <switch.h>

#include <chrono>
#include <cstdlib>
#include <format>
#include <limits>
#include <string>
#include <string_view>

namespace
{
  constexpr double       BytesPerMebibyte          = 1024.0 * 1024.0;
  constexpr double       MillisecondsPerSecond     = 1'000.0;
  constexpr double       NanosecondsPerMillisecond = 1'000'000.0;
  constexpr std::int32_t StickDirectionThreshold   = JOYSTICK_MAX / 4;
  constexpr auto         AudioTelemetryInterval    = std::chrono::seconds( 5 );

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

  [[nodiscard]] std::string formatResult( const std::uint32_t result )
  {
    return std::format( "0x{:08X}", result );
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

  void logAudioTelemetry( doggo::log::Logger &                                  logger,
                          const doggo::platform::nx::AudrenTelemetry &          telemetry,
                          const doggo::platform::nx::MonotonicClock::time_point timestamp,
                          const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    const double bufferedMilliseconds = static_cast<double>( telemetry.buffered_sample_count ) *
                                        MillisecondsPerSecond /
                                        doggo::platform::nx::AudrenTone::SampleRate;
    const double minimumBufferedMilliseconds = static_cast<double>( telemetry.minimum_buffered_sample_count ) *
                                               MillisecondsPerSecond /
                                               doggo::platform::nx::AudrenTone::SampleRate;

    writeLog( logger,
              doggo::log::Level::Info,
              "Audio",
              std::format( "Frames: {}, samples: {}, buffers: {}/{} (low {}), buffered: {:.1f} ms (low {:.1f}), "
                           "underruns: {}, voice drops: {}, "
                           "late wakes: {}, max gap: {:.3f} ms, max update: {:.3f} ms",
                           telemetry.renderer_frame_count,
                           telemetry.played_sample_count,
                           telemetry.queued_buffer_count,
                           doggo::platform::nx::AudrenTone::BufferCount,
                           telemetry.minimum_buffer_count,
                           bufferedMilliseconds,
                           minimumBufferedMilliseconds,
                           telemetry.buffer_underrun_count,
                           telemetry.voice_drop_count,
                           telemetry.late_wakeup_count,
                           static_cast<double>( telemetry.maximum_wakeup_gap_ns ) / NanosecondsPerMillisecond,
                           static_cast<double>( telemetry.maximum_update_time_ns ) / NanosecondsPerMillisecond ),
              timestamp,
              startedAt );
  }

  [[nodiscard]] bool logAudioFaultChanges( doggo::log::Logger &                                  logger,
                                           const doggo::platform::nx::AudrenTelemetry &          current,
                                           const doggo::platform::nx::AudrenTelemetry &          previous,
                                           const doggo::platform::nx::MonotonicClock::time_point timestamp,
                                           const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    bool hasUnexpectedFailure = false;

    if ( current.buffer_underrun_count != previous.buffer_underrun_count )
    {
      const bool isInjected = current.buffer_underrun_count <= current.injected_stall_count;
      writeLog( logger,
                isInjected ? doggo::log::Level::Warning : doggo::log::Level::Error,
                "Audio",
                std::format( "{} buffer underrun detected (total {}, injected stalls {})",
                             isInjected ? "Injected" : "Unexpected",
                             current.buffer_underrun_count,
                             current.injected_stall_count ),
                timestamp,
                startedAt );
      hasUnexpectedFailure = !isInjected;
    }

    if ( current.voice_drop_count != previous.voice_drop_count )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "Audio",
                std::format( "audren voice drops increased to {}", current.voice_drop_count ),
                timestamp,
                startedAt );
      hasUnexpectedFailure = true;
    }

    if ( current.update_failure_count != previous.update_failure_count )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "Audio",
                std::format( "Driver update failed: {} (failures {})",
                             formatResult( current.last_update_result ),
                             current.update_failure_count ),
                timestamp,
                startedAt );
      hasUnexpectedFailure = true;
    }

    if ( current.wait_failure_count != previous.wait_failure_count )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "Audio",
                std::format( "Frame-event wait failed: {} (failures {})",
                             formatResult( current.last_wait_result ),
                             current.wait_failure_count ),
                timestamp,
                startedAt );
      hasUnexpectedFailure = true;
    }

    return hasUnexpectedFailure;
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

    log::Logger     logger;
    ConsoleLogSink  consoleSink;
    DoggoDevLogSink doggoDevSink;
    if ( !logger.attach( consoleSink ) )
    {
      consoleExit( nullptr );
      return EXIT_FAILURE;
    }

    const bool isDoggoDevConnected = doggoDevSink.initialize() && logger.attach( doggoDevSink );
    if ( isDoggoDevConnected )
    {
      writeLog( logger,
                log::Level::Info,
                "Logging",
                "PC log stream connected through DoggoDev",
                MonotonicClock::now(),
                startedAt );
    }
    else
    {
      doggoDevSink.finalize();
      writeLog( logger,
                log::Level::Warning,
                "Logging",
                "PC log stream unavailable; deploy with DoggoDev to attach it",
                MonotonicClock::now(),
                startedAt );
    }

    AppletLifecycle     lifecycle;
    const std::uint32_t lifecycleResult = lifecycle.initialize();

    Input input;
    input.initialize();

    AudrenTone          audio;
    const std::uint32_t audioResult = audio.initialize();

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

    if ( R_SUCCEEDED( audioResult ) )
    {
      writeLog( logger,
                log::Level::Info,
                "Audio",
                std::format( "audren started: {} Hz, {} samples/frame, {} buffers ({} ms), {} Hz tone",
                             AudrenTone::SampleRate,
                             AudrenTone::SamplesPerBuffer,
                             AudrenTone::BufferCount,
                             AudrenTone::BufferCount * AUDREN_TIMER_PERIOD_MS,
                             AudrenTone::ToneFrequency ),
                MonotonicClock::now(),
                startedAt );
      writeLog( logger,
                log::Level::Info,
                "Audio",
                std::format( "Frame-event service thread: core {}, priority 0x{:02X}",
                             AudrenTone::ThreadCore,
                             AudrenTone::ThreadPriority ),
                MonotonicClock::now(),
                startedAt );
    }
    else
    {
      writeLog( logger,
                log::Level::Error,
                "Audio",
                std::format( "Initialization failed: {}", formatResult( audioResult ) ),
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
              "Press (A) for a 30 ms audio stall and (+) to exit",
              MonotonicClock::now(),
              startedAt );

    MonotonicClock::time_point previousTime       = startedAt;
    MonotonicClock::time_point nextAudioTelemetry = startedAt + AudioTelemetryInterval;
    bool                       isRunning          = true;
    AppletFocusState           focusState         = AppletFocusState_InFocus;
    InputTelemetryState        inputTelemetry;
    AudrenTelemetry            previousAudioTelemetry = audio.telemetry();

    while ( isRunning )
    {
      isRunning = appletMainLoop();

      AppletLifecycleEvent lifecycleEvent;
      while ( lifecycle.tryPopEvent( lifecycleEvent ) )
      {
        if ( lifecycleEvent.type == AppletLifecycleEventType::FocusStateChanged )
        {
          focusState = static_cast<AppletFocusState>( lifecycleEvent.detail );
          audio.requestPause( focusState != AppletFocusState_InFocus );
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

      if ( audio.isInitialized() && ( inputSnapshot.buttons_down & HidNpadButton_A ) != 0 )
      {
        writeLog( logger,
                  log::Level::Warning,
                  "Audio",
                  "Injecting a 30 ms service-thread stall; one detected underrun is expected",
                  currentTime,
                  startedAt );
        audio.requestUnderrunTest();
      }

      if ( audio.isInitialized() )
      {
        const AudrenTelemetry currentAudioTelemetry = audio.telemetry();
        if ( logAudioFaultChanges( logger, currentAudioTelemetry, previousAudioTelemetry, currentTime, startedAt ) )
        {
          exitCode = EXIT_FAILURE;
        }

        if ( currentTime >= nextAudioTelemetry )
        {
          logAudioTelemetry( logger, currentAudioTelemetry, currentTime, startedAt );
          nextAudioTelemetry = currentTime + AudioTelemetryInterval;
        }
        previousAudioTelemetry = currentAudioTelemetry;
      }

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

    if ( audio.isInitialized() )
    {
      audio.finalize();
      const AudrenTelemetry finalAudioTelemetry = audio.telemetry();
      if ( logAudioFaultChanges( logger,
                                 finalAudioTelemetry,
                                 previousAudioTelemetry,
                                 MonotonicClock::now(),
                                 startedAt ) )
      {
        exitCode = EXIT_FAILURE;
      }
      logAudioTelemetry( logger, finalAudioTelemetry, MonotonicClock::now(), startedAt );

      if ( finalAudioTelemetry.buffer_underrun_count != finalAudioTelemetry.injected_stall_count )
      {
        writeLog( logger,
                  log::Level::Error,
                  "Audio",
                  std::format( "Underrun proof mismatch: {} detected for {} injected stalls",
                               finalAudioTelemetry.buffer_underrun_count,
                               finalAudioTelemetry.injected_stall_count ),
                  MonotonicClock::now(),
                  startedAt );
        exitCode = EXIT_FAILURE;
      }

      if ( finalAudioTelemetry.voice_drop_count != 0 ||
           finalAudioTelemetry.update_failure_count != 0 ||
           finalAudioTelemetry.wait_failure_count != 0 )
      {
        exitCode = EXIT_FAILURE;
      }
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
    logger.detach( doggoDevSink );
    doggoDevSink.finalize();
    logger.detach( consoleSink );
    consoleExit( nullptr );
    return exitCode;
  }
}  // namespace doggo::platform::nx
