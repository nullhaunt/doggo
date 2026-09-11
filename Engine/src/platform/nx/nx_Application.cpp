#include "doggo/platform/nx/nx_Application.hpp"

#include "doggo/log/log_Log.hpp"
#include "doggo/platform/nx/nx_AppletLifecycle.hpp"
#include "doggo/platform/nx/nx_ApplicationStorage.hpp"
#include "doggo/platform/nx/nx_AudrenTone.hpp"
#include "doggo/platform/nx/nx_Input.hpp"
#include "doggo/platform/nx/nx_LogSinks.hpp"
#include "doggo/platform/nx/nx_Memory.hpp"
#include "doggo/platform/nx/nx_MonotonicClock.hpp"
#include "doggo/platform/nx/nx_RomFs.hpp"

#include <switch.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace
{
  constexpr double       BytesPerMebibyte            = 1024.0 * 1024.0;
  constexpr double       MillisecondsPerSecond       = 1'000.0;
  constexpr double       NanosecondsPerMillisecond   = 1'000'000.0;
  constexpr std::int32_t StickDirectionThreshold     = JOYSTICK_MAX / 4;
  constexpr auto         AudioTelemetryInterval      = std::chrono::seconds( 5 );
  constexpr char         RomFsFixturePath[]          = "romfs:/gate0/read_fixture.bin";
  constexpr char         SaveFixtureName[]           = "gate0_save_fixture.bin";
  constexpr std::size_t  SaveFixtureSize             = 64;
  constexpr std::size_t  SaveFixtureGenerationOffset = 16;
  constexpr std::size_t  SaveFixtureChecksumOffset   = 56;

  constexpr std::array<std::uint8_t, 64> ExpectedRomFsFixture = {
      0x44, 0x4F, 0x47, 0x47, 0x4F, 0x52, 0x46, 0x53, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
      0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  };
  constexpr std::uint64_t ExpectedRomFsFixtureHash = 0x01BC5E25326D5821;

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

  [[nodiscard]] constexpr std::uint64_t fnv1a64( const std::span<const std::uint8_t> bytes ) noexcept
  {
    std::uint64_t hash = 0xCBF29CE484222325;
    for ( const std::uint8_t byte : bytes )
    {
      hash ^= byte;
      hash *= 0x100000001B3;
    }

    return hash;
  }

  static_assert( fnv1a64( std::span<const std::uint8_t>{ ExpectedRomFsFixture } ) == ExpectedRomFsFixtureHash );

  constexpr void writeLittleEndian32( std::array<std::uint8_t, SaveFixtureSize> & destination,
                                      const std::size_t                           offset,
                                      const std::uint32_t                         value ) noexcept
  {
    for ( std::size_t index = 0; index < sizeof( value ); ++index )
    {
      destination[ offset + index ] = static_cast<std::uint8_t>( value >> ( index * 8 ) );
    }
  }

  constexpr void writeLittleEndian64( std::array<std::uint8_t, SaveFixtureSize> & destination,
                                      const std::size_t                           offset,
                                      const std::uint64_t                         value ) noexcept
  {
    for ( std::size_t index = 0; index < sizeof( value ); ++index )
    {
      destination[ offset + index ] = static_cast<std::uint8_t>( value >> ( index * 8 ) );
    }
  }

  [[nodiscard]] constexpr std::uint64_t readLittleEndian64( const std::array<std::uint8_t, SaveFixtureSize> & source,
                                                            const std::size_t offset ) noexcept
  {
    std::uint64_t value = 0;
    for ( std::size_t index = 0; index < sizeof( value ); ++index )
    {
      value |= static_cast<std::uint64_t>( source[ offset + index ] ) << ( index * 8 );
    }

    return value;
  }

  [[nodiscard]] constexpr std::array<std::uint8_t, SaveFixtureSize>
  makeSaveFixture( const std::uint64_t generation ) noexcept
  {
    constexpr std::array<std::uint8_t, 8> magic = { 0x44, 0x4F, 0x47, 0x47, 0x4F, 0x53, 0x41, 0x56 };

    std::array<std::uint8_t, SaveFixtureSize> fixture = {};
    for ( std::size_t index = 0; index < magic.size(); ++index )
    {
      fixture[ index ] = magic[ index ];
    }

    writeLittleEndian32( fixture, 8, 1 );
    writeLittleEndian32( fixture, 12, static_cast<std::uint32_t>( SaveFixtureSize ) );
    writeLittleEndian64( fixture, SaveFixtureGenerationOffset, generation );

    for ( std::size_t index = 0; index < 32; ++index )
    {
      fixture[ 24 + index ] = static_cast<std::uint8_t>( 0xA0 + index );
    }

    const std::uint64_t checksum =
        fnv1a64( std::span<const std::uint8_t>{ fixture.data(), SaveFixtureChecksumOffset } );
    writeLittleEndian64( fixture, SaveFixtureChecksumOffset, checksum );
    return fixture;
  }

  constexpr auto FirstSaveFixture = makeSaveFixture( 1 );
  static_assert( readLittleEndian64( FirstSaveFixture, SaveFixtureChecksumOffset ) == 0x65FCF37B61C6B04B );
  static_assert( fnv1a64( std::span<const std::uint8_t>{ FirstSaveFixture } ) == 0xCC89D9B280F89DE4 );

  [[nodiscard]] const char *
  getApplicationStorageStatusName( const doggo::platform::nx::ApplicationStorageStatus status ) noexcept
  {
    using Status = doggo::platform::nx::ApplicationStorageStatus;
    switch ( status )
    {
      case Status::Success:
        return "Success";

      case Status::NotInitialized:
        return "Not Initialized";

      case Status::InvalidName:
        return "Invalid Name";

      case Status::NotFound:
        return "Not Found";

      case Status::DirectoryFailed:
        return "Directory Failed";

      case Status::StatFailed:
        return "Stat Failed";

      case Status::DestinationTooSmall:
        return "Destination too Small";

      case Status::OpenFailed:
        return "Open Failed";

      case Status::ReadFailed:
        return "Read Failed";

      case Status::WriteFailed:
        return "Write Failed";

      case Status::FlushFailed:
        return "Flush Failed";

      case Status::CloseFailed:
        return "Close Failed";

      case Status::RemoveFailed:
        return "Remove Failed";

      case Status::RenameFailed:
        return "Rename Failed";

      case Status::CommitFailed:
        return "Commit Failed";
    }

    return "Unknown";
  }

  [[nodiscard]] const char * getRomFsReadStatusName( const doggo::platform::nx::RomFsReadStatus status ) noexcept
  {
    using Status = doggo::platform::nx::RomFsReadStatus;
    switch ( status )
    {
      case Status::Success:
        return "Success";

      case Status::NotInitialized:
        return "Not Initialized";

      case Status::InvalidArgument:
        return "Invalid Argument";

      case Status::OpenFailed:
        return "Open Failed";

      case Status::StatFailed:
        return "Stat Failed";

      case Status::DestinationTooSmall:
        return "Destination too Small";

      case Status::ReadFailed:
        return "Read Failed";

      case Status::CloseFailed:
        return "Close Failed";
    }

    return "Unknown";
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

  [[nodiscard]] bool isValidRomFsFixture( doggo::log::Logger &                                  logger,
                                          doggo::platform::nx::RomFs &                          romFs,
                                          const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    const std::uint32_t mountResult = romFs.initialize();
    if ( R_FAILED( mountResult ) )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "FileSystem",
                std::format( "ROMFS mount failed: {}", formatResult( mountResult ) ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    std::array<std::uint8_t, ExpectedRomFsFixture.size()> fixtureBytes = {};
    const doggo::platform::nx::RomFsReadReport            report = romFs.readFile( RomFsFixturePath, fixtureBytes );
    if ( report.status != doggo::platform::nx::RomFsReadStatus::Success )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "FileSystem",
                std::format( "ROMFS fixture read failed: {} (errno {}, {} of {} bytes)",
                             getRomFsReadStatusName( report.status ),
                             report.error_number,
                             report.bytes_read,
                             report.file_size ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    const std::uint64_t actualHash = fnv1a64( std::span<const std::uint8_t>{ fixtureBytes.data(), report.bytes_read } );
    const bool          isExactSize  = report.file_size == ExpectedRomFsFixture.size();
    const bool          isExactBytes = isExactSize && fixtureBytes == ExpectedRomFsFixture;
    const bool          isExactHash  = actualHash == ExpectedRomFsFixtureHash;
    if ( !isExactBytes || !isExactHash )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "FileSystem",
                std::format( "ROMFS fixture validation failed: expected {} bytes/FNV-1a 0x{:016X}, "
                             "got {} bytes/FNV-1a 0x{:016X}",
                             ExpectedRomFsFixture.size(),
                             ExpectedRomFsFixtureHash,
                             report.file_size,
                             actualHash ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    writeLog(
        logger,
        doggo::log::Level::Info,
        "FileSystem",
        std::format( "Validated {}: {} exact bytes, FNV-1a 0x{:016X}", RomFsFixturePath, report.file_size, actualHash ),
        doggo::platform::nx::MonotonicClock::now(),
        startedAt );
    return true;
  }

  void logApplicationStorageFailure( doggo::log::Logger &                                  logger,
                                     const std::string_view                                action,
                                     const doggo::platform::nx::ApplicationStorageReport & report,
                                     const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    writeLog( logger,
              doggo::log::Level::Error,
              "FileSystem",
              std::format( "{}: {} (errno {}, result {}, {} of {} bytes)",
                           action,
                           getApplicationStorageStatusName( report.status ),
                           report.error_number,
                           formatResult( report.native_result ),
                           report.bytes_transferred,
                           report.file_size ),
              doggo::platform::nx::MonotonicClock::now(),
              startedAt );
  }

  [[nodiscard]] bool isValidCommittedSaveFixture( doggo::log::Logger &                                  logger,
                                                  doggo::platform::nx::ApplicationStorage &             storage,
                                                  const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    const std::uint32_t mountResult = storage.initialize();
    if ( R_FAILED( mountResult ) )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "FileSystem",
                std::format( "Writable storage initialization failed: {}", formatResult( mountResult ) ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    std::uint64_t                                       previousGeneration = 0;
    std::array<std::uint8_t, SaveFixtureSize>           previousFixture    = {};
    const doggo::platform::nx::ApplicationStorageReport previousReport =
        storage.readCommittedFile( SaveFixtureName, previousFixture );

    if ( previousReport.status == doggo::platform::nx::ApplicationStorageStatus::Success )
    {
      const std::uint64_t storedGeneration = readLittleEndian64( previousFixture, SaveFixtureGenerationOffset );
      const auto          expectedPrevious = makeSaveFixture( storedGeneration );
      if ( storedGeneration == 0 ||
           previousReport.file_size != previousFixture.size() ||
           previousReport.bytes_transferred != previousFixture.size() ||
           previousFixture != expectedPrevious )
      {
        const std::uint64_t actualHash =
            fnv1a64( std::span<const std::uint8_t>{ previousFixture.data(), previousReport.bytes_transferred } );
        writeLog( logger,
                  doggo::log::Level::Error,
                  "FileSystem",
                  std::format( "Committed save fixture is invalid: {} bytes, FNV-1a 0x{:016X}",
                               previousReport.file_size,
                               actualHash ),
                  doggo::platform::nx::MonotonicClock::now(),
                  startedAt );
        return false;
      }

      previousGeneration = storedGeneration;
      writeLog( logger,
                doggo::log::Level::Info,
                "FileSystem",
                std::format( "Previous committed save fixture valid: generation {}", previousGeneration ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
    }
    else if ( previousReport.status != doggo::platform::nx::ApplicationStorageStatus::NotFound )
    {
      logApplicationStorageFailure( logger, "Committed save fixture read failed", previousReport, startedAt );
      return false;
    }

    if ( previousGeneration == std::numeric_limits<std::uint64_t>::max() )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "FileSystem",
                "Committed save fixture generation exhausted",
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    const std::uint64_t                                 nextGeneration  = previousGeneration + 1;
    const auto                                          expectedFixture = makeSaveFixture( nextGeneration );
    const doggo::platform::nx::ApplicationStorageReport writeReport =
        storage.writeCommittedFile( SaveFixtureName, expectedFixture );
    if ( writeReport.status != doggo::platform::nx::ApplicationStorageStatus::Success )
    {
      logApplicationStorageFailure( logger, "Committed save fixture write failed", writeReport, startedAt );
      return false;
    }

    std::array<std::uint8_t, SaveFixtureSize>           readBackFixture = {};
    const doggo::platform::nx::ApplicationStorageReport readBackReport =
        storage.readCommittedFile( SaveFixtureName, readBackFixture );
    if ( readBackReport.status != doggo::platform::nx::ApplicationStorageStatus::Success )
    {
      logApplicationStorageFailure( logger, "Committed save fixture read-back failed", readBackReport, startedAt );
      return false;
    }

    const std::uint64_t actualHash =
        fnv1a64( std::span<const std::uint8_t>{ readBackFixture.data(), readBackReport.bytes_transferred } );
    const bool isExactSize = readBackReport.file_size == expectedFixture.size() &&
                             readBackReport.bytes_transferred == expectedFixture.size();
    if ( !isExactSize || readBackFixture != expectedFixture )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "FileSystem",
                std::format( "Committed save fixture validation failed: expected {} bytes, "
                             "got {} bytes/FNV-1a 0x{:016X}",
                             expectedFixture.size(),
                             readBackReport.file_size,
                             actualHash ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    writeLog( logger,
              doggo::log::Level::Info,
              "FileSystem",
              std::format( "Committed {}/{} via {}: generation {}, {} exact bytes, FNV-1a 0x{:016X}",
                           storage.rootPath(),
                           SaveFixtureName,
                           storage.backendName(),
                           nextGeneration,
                           readBackReport.file_size,
                           actualHash ),
              doggo::platform::nx::MonotonicClock::now(),
              startedAt );
    return true;
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

    RomFs romFs;
    if ( !isValidRomFsFixture( logger, romFs, startedAt ) )
    {
      exitCode = EXIT_FAILURE;
    }

    ApplicationStorage storage;
    if ( !isValidCommittedSaveFixture( logger, storage, startedAt ) )
    {
      exitCode = EXIT_FAILURE;
    }

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

    const std::uint32_t storageFinalizeResult = storage.finalize();
    if ( R_FAILED( storageFinalizeResult ) )
    {
      writeLog( logger,
                log::Level::Error,
                "FileSystem",
                std::format( "Writable storage finalization failed: {}", formatResult( storageFinalizeResult ) ),
                MonotonicClock::now(),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    const std::uint32_t romFsFinalizeResult = romFs.finalize();
    if ( R_FAILED( romFsFinalizeResult ) )
    {
      writeLog( logger,
                log::Level::Error,
                "FileSystem",
                std::format( "ROMFS unmount failed: {}", formatResult( romFsFinalizeResult ) ),
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
    logger.detach( doggoDevSink );
    doggoDevSink.finalize();
    logger.detach( consoleSink );
    consoleExit( nullptr );
    return exitCode;
  }
}  // namespace doggo::platform::nx
