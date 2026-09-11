#include "doggo/platform/nx/nx_Application.hpp"

#include "doggo/gpu/deko/deko_GraphicsContext.hpp"
#include "doggo/gpu/deko/deko_GraphicsProgram.hpp"
#include "doggo/gpu/deko/deko_Presenter.hpp"
#include "doggo/log/log_Log.hpp"
#include "doggo/platform/nx/nx_AppletLifecycle.hpp"
#include "doggo/platform/nx/nx_AudrenTone.hpp"
#include "doggo/platform/nx/nx_Input.hpp"
#include "doggo/platform/nx/nx_LogSinks.hpp"
#include "doggo/platform/nx/nx_Memory.hpp"
#include "doggo/platform/nx/nx_MonotonicClock.hpp"
#include "doggo/platform/nx/nx_RomFs.hpp"
#include "doggo/platform/nx/nx_SdCardSaveStorage.hpp"
#include "doggo/save/save_Gate0Fixture.hpp"

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
  constexpr double        BytesPerMebibyte             = 1024.0 * 1024.0;
  constexpr double        MillisecondsPerSecond        = 1'000.0;
  constexpr double        NanosecondsPerMillisecond    = 1'000'000.0;
  constexpr std::int32_t  StickDirectionThreshold      = JOYSTICK_MAX / 4;
  constexpr auto          AudioTelemetryInterval       = std::chrono::seconds( 5 );
  constexpr char          RomFsFixturePath[]           = "romfs:/gate0/read_fixture.bin";
  constexpr char          TriangleVertexShaderPath[]   = "romfs:/shaders/gate0/doggo_gate0_triangle_vsh.dksh";
  constexpr char          TriangleFragmentShaderPath[] = "romfs:/shaders/gate0/doggo_gate0_triangle_fsh.dksh";
  constexpr std::size_t   Gate0ShaderBinaryCapacity    = static_cast<const std::size_t>( 64u * 1024u );
  constexpr std::uint32_t HandheldWidth                = 1280;
  constexpr std::uint32_t HandheldHeight               = 720;
  constexpr std::uint32_t DockedWidth                  = 1920;
  constexpr std::uint32_t DockedHeight                 = 1080;

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

  [[nodiscard]] const char * getSaveStorageStatusName( const doggo::save::SaveStorageStatus status ) noexcept
  {
    using Status = doggo::save::SaveStorageStatus;
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

  [[nodiscard]] doggo::gpu::deko::PresentationReport
  renderTriangleFrame( doggo::gpu::deko::Presenter &             presenter,
                       const doggo::gpu::deko::GraphicsProgram & graphicsProgram ) noexcept
  {
    doggo::gpu::deko::PresentationFrame  frame;
    doggo::gpu::deko::PresentationReport report = presenter.beginFrame( frame );

    if ( report.status != doggo::gpu::deko::PresentationStatus::Success )
    {
      return report;
    }

    const dk::ImageView colorTarget{ *frame.color_image };
    frame.command_buffer.bindRenderTargets( &colorTarget );
    frame.command_buffer.setViewports( 0,
                                       DkViewport{
                                           .x      = 0.0f,
                                           .y      = 0.0f,
                                           .width  = static_cast<float>( frame.extent.width ),
                                           .height = static_cast<float>( frame.extent.height ),
                                           .near   = 0.0f,
                                           .far    = 1.0f,
                                       } );
    frame.command_buffer.setScissors(
        0,
        DkScissor{ .x = 0, .y = 0, .width = frame.extent.width, .height = frame.extent.height } );
    frame.command_buffer.clearColor( 0, DkColorMask_RGBA, 0.5f, 0.5f, 0.5f, 1.0f );

    graphicsProgram.bind( frame.command_buffer );
    dk::RasterizerState rasterizerState;
    rasterizerState.setCullMode( DkFace_None );
    frame.command_buffer.bindRasterizerState( rasterizerState );
    frame.command_buffer.bindColorState( dk::ColorState{} );
    frame.command_buffer.bindColorWriteState( dk::ColorWriteState{} );
    frame.command_buffer.draw( DkPrimitive_Triangles, 3, 1, 0, 0 );
    return presenter.endFrame();
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

  [[nodiscard]] bool readShaderBinary( doggo::log::Logger &                                  logger,
                                       const doggo::platform::nx::RomFs &                    romFs,
                                       const char * const                                    path,
                                       const std::span<std::uint8_t>                         destination,
                                       std::size_t &                                         binarySize,
                                       const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    const doggo::platform::nx::RomFsReadReport report = romFs.readFile( path, destination );
    if ( report.status != doggo::platform::nx::RomFsReadStatus::Success )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "GPU",
                std::format( "Shader read failed for {}: {} (errno {}, {} of {} bytes)",
                             path,
                             getRomFsReadStatusName( report.status ),
                             report.error_number,
                             report.bytes_read,
                             report.file_size ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    if ( report.bytes_read == 0 )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "GPU",
                std::format( "Shader is empty: {}", path ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    binarySize = report.bytes_read;
    return true;
  }

  [[nodiscard]] bool initializeGate0TriangleProgram( doggo::log::Logger &                logger,
                                                     const doggo::platform::nx::RomFs &  romFs,
                                                     doggo::gpu::deko::GraphicsProgram & graphicsProgram,
                                                     const dk::Device                    device,
                                                     const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    std::array<std::uint8_t, Gate0ShaderBinaryCapacity> vertexBinary   = {};
    std::array<std::uint8_t, Gate0ShaderBinaryCapacity> fragmentBinary = {};
    std::size_t                                         vertexSize     = 0;
    std::size_t                                         fragmentSize   = 0;

    if ( !readShaderBinary( logger, romFs, TriangleVertexShaderPath, vertexBinary, vertexSize, startedAt ) ||
         !readShaderBinary( logger, romFs, TriangleFragmentShaderPath, fragmentBinary, fragmentSize, startedAt ) )
    {
      return false;
    }

    const doggo::gpu::deko::GraphicsProgramStatus status =
        graphicsProgram.initialize( device,
                                    std::span<const std::uint8_t>{ vertexBinary.data(), vertexSize },
                                    std::span<const std::uint8_t>{ fragmentBinary.data(), fragmentSize } );
    if ( status != doggo::gpu::deko::GraphicsProgramStatus::Success )
    {
      writeLog( logger,
                doggo::log::Level::Error,
                "GPU",
                std::format( "Triangle graphics program initialization failed: {}",
                             doggo::gpu::deko::getGraphicsProgramStatusName( status ) ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
      return false;
    }

    writeLog( logger,
              doggo::log::Level::Info,
              "GPU",
              std::format( "Triangle graphics program initialized: {}-byte vertex DKSH, "
                           "{}-byte fragment DKSH, {}-byte code block",
                           vertexSize,
                           fragmentSize,
                           graphicsProgram.codeMemorySize() ),
              doggo::platform::nx::MonotonicClock::now(),
              startedAt );
    return true;
  }

  void logSaveStorageFailure( doggo::log::Logger &                                  logger,
                              const std::string_view                                action,
                              const doggo::save::SaveStorageReport &                report,
                              const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    writeLog( logger,
              doggo::log::Level::Error,
              "FileSystem",
              std::format( "{}: {} (errno {}, result {}, {} of {} bytes)",
                           action,
                           getSaveStorageStatusName( report.status ),
                           report.error_number,
                           formatResult( report.native_result ),
                           report.bytes_transferred,
                           report.file_size ),
              doggo::platform::nx::MonotonicClock::now(),
              startedAt );
  }

  [[nodiscard]] bool isValidCommittedSaveFixture( doggo::log::Logger &                                  logger,
                                                  doggo::save::SaveStorage &                            storage,
                                                  const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    const doggo::save::Gate0FixtureReport report = doggo::save::runGate0Fixture( storage );
    using Status                                 = doggo::save::Gate0FixtureStatus;

    switch ( report.status )
    {
      case Status::StorageInitializationFailed:
        writeLog(
            logger,
            doggo::log::Level::Error,
            "FileSystem",
            std::format( "Writable storage initialization failed: {}", formatResult( report.storage.native_result ) ),
            doggo::platform::nx::MonotonicClock::now(),
            startedAt );
        return false;

      case Status::PreviousReadFailed:
        logSaveStorageFailure( logger, "Committed save fixture read failed", report.storage, startedAt );
        return false;

      case Status::PreviousFixtureInvalid:
        writeLog( logger,
                  doggo::log::Level::Error,
                  "FileSystem",
                  std::format( "Committed save fixture is invalid: {} bytes, FNV-1a 0x{:016X}",
                               report.storage.file_size,
                               report.content_hash ),
                  doggo::platform::nx::MonotonicClock::now(),
                  startedAt );
        return false;

      case Status::GenerationExhausted:
        writeLog( logger,
                  doggo::log::Level::Error,
                  "FileSystem",
                  "Committed save fixture generation exhausted",
                  doggo::platform::nx::MonotonicClock::now(),
                  startedAt );
        return false;

      case Status::WriteFailed:
        logSaveStorageFailure( logger, "Committed save fixture write failed", report.storage, startedAt );
        return false;

      case Status::ReadBackFailed:
        logSaveStorageFailure( logger, "Committed save fixture read-back failed", report.storage, startedAt );
        return false;

      case Status::ReadBackInvalid:
        writeLog( logger,
                  doggo::log::Level::Error,
                  "FileSystem",
                  std::format( "Committed save fixture validation failed: expected {} bytes, "
                               "got {} bytes/FNV-1a 0x{:016X}",
                               doggo::save::Gate0FixtureSize,
                               report.storage.file_size,
                               report.content_hash ),
                  doggo::platform::nx::MonotonicClock::now(),
                  startedAt );
        return false;

      case Status::Success:
        break;
    }

    if ( report.had_previous_fixture )
    {
      writeLog( logger,
                doggo::log::Level::Info,
                "FileSystem",
                std::format( "Previous committed save fixture valid: generation {}", report.previous_generation ),
                doggo::platform::nx::MonotonicClock::now(),
                startedAt );
    }

    writeLog( logger,
              doggo::log::Level::Info,
              "FileSystem",
              std::format( "Committed {}/{} via {}: generation {}, {} exact bytes, FNV-1a 0x{:016X}",
                           storage.rootPath(),
                           doggo::save::Gate0FixtureName,
                           storage.backendName(),
                           report.committed_generation,
                           report.storage.file_size,
                           report.content_hash ),
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

  [[nodiscard]] doggo::gpu::deko::PresentationExtent getInitialPresentationExtent() noexcept
  {
    if ( appletGetOperationMode() == AppletOperationMode_Console )
    {
      return { .width = DockedWidth, .height = DockedHeight };
    }

    return { .width = HandheldWidth, .height = HandheldHeight };
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
    const MonotonicClock::time_point startedAt = MonotonicClock::now();

    log::Logger     logger;
    DoggoDevLogSink doggoDevSink;

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

    gpu::deko::GraphicsContext             graphicsContext;
    const gpu::deko::GraphicsContextStatus graphicsStatus = graphicsContext.initialize( logger );
    gpu::deko::GraphicsProgram             triangleProgram;
    gpu::deko::Presenter                   presenter;
    gpu::deko::PresentationReport          presentationReport = {
                 .status = gpu::deko::PresentationStatus::NotInitialized,
    };

    const gpu::deko::PresentationExtent presentationExtent = getInitialPresentationExtent();
    if ( graphicsStatus == gpu::deko::GraphicsContextStatus::Success )
    {
      presentationReport = presenter.initialize( graphicsContext.device(),
                                                 graphicsContext.graphicsQueue(),
                                                 nwindowGetDefault(),
                                                 presentationExtent );
    }

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

    if ( graphicsStatus == gpu::deko::GraphicsContextStatus::Success )
    {
      writeLog( logger,
                log::Level::Info,
                "GPU",
                "deko3d device and graphics queue initialized: depth [0, 1], upper-left origin",
                MonotonicClock::now(),
                startedAt );
    }
    else
    {
      writeLog(
          logger,
          log::Level::Error,
          "GPU",
          std::format( "Initialization failed: {}", doggo::gpu::deko::getGraphicsContextStatusName( graphicsStatus ) ),
          MonotonicClock::now(),
          startedAt );
      exitCode = EXIT_FAILURE;
    }

    if ( presentationReport.status == gpu::deko::PresentationStatus::Success )
    {
      writeLog( logger,
                log::Level::Info,
                "GPU",
                std::format( "Presentation initialized: {}x{}, {} images, {} frame contexts, swap interval {}",
                             presentationExtent.width,
                             presentationExtent.height,
                             gpu::deko::Presenter::FrameCount,
                             gpu::deko::Presenter::FrameCount,
                             gpu::deko::Presenter::SwapInterval ),
                MonotonicClock::now(),
                startedAt );
    }
    else
    {
      writeLog( logger,
                log::Level::Error,
                "GPU",
                std::format( "Presentation initialization failed: {} (context {})",
                             gpu::deko::getPresentationStatusName( presentationReport.status ),
                             presentationReport.context_index ),
                MonotonicClock::now(),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    RomFs romFs;
    if ( !isValidRomFsFixture( logger, romFs, startedAt ) )
    {
      exitCode = EXIT_FAILURE;
    }
    else if ( graphicsStatus == gpu::deko::GraphicsContextStatus::Success &&
              !initializeGate0TriangleProgram( logger, romFs, triangleProgram, graphicsContext.device(), startedAt ) )
    {
      exitCode = EXIT_FAILURE;
    }

    SdCardSaveStorage storage;
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
    bool                       isRunning          = presenter.isInitialized() && triangleProgram.isInitialized();
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

      const gpu::deko::PresentationReport frameReport = renderTriangleFrame( presenter, triangleProgram );
      if ( frameReport.status != gpu::deko::PresentationStatus::Success )
      {
        writeLog( logger,
                  log::Level::Error,
                  "GPU",
                  std::format( "Frame presentation failed: {} (deko result {}, context {}, image {})",
                               gpu::deko::getPresentationStatusName( frameReport.status ),
                               static_cast<std::uint32_t>( frameReport.deko_result ),
                               frameReport.context_index,
                               frameReport.image_slot ),
                  currentTime,
                  startedAt );
        exitCode = EXIT_FAILURE;
        break;
      }
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

    const bool                          wasPresentationInitialized = presenter.isInitialized();
    const gpu::deko::PresentationReport presentationFinalizeReport = presenter.finalize();

    if ( presentationFinalizeReport.status != gpu::deko::PresentationStatus::Success )
    {
      writeLog( logger,
                log::Level::Error,
                "GPU",
                std::format( "Presentation finalization failed: {} (deko result {}, context {})",
                             gpu::deko::getPresentationStatusName( presentationFinalizeReport.status ),
                             static_cast<std::uint32_t>( presentationFinalizeReport.deko_result ),
                             presentationFinalizeReport.context_index ),
                MonotonicClock::now(),
                startedAt );
      exitCode = EXIT_FAILURE;
    }
    else if ( wasPresentationInitialized )
    {
      writeLog( logger,
                log::Level::Info,
                "GPU",
                "Presentation images and frame contexts finalized",
                MonotonicClock::now(),
                startedAt );
    }

    const bool wasTriangleProgramInitialized = triangleProgram.isInitialized();
    triangleProgram.finalize();

    if ( wasTriangleProgramInitialized )
    {
      writeLog( logger,
                log::Level::Info,
                "GPU",
                "Triangle graphics program finalized",
                MonotonicClock::now(),
                startedAt );
    }

    const bool wasGraphicsInitialized = graphicsContext.isInitialized();
    graphicsContext.finalize();

    if ( wasGraphicsInitialized )
    {
      writeLog( logger,
                log::Level::Info,
                "GPU",
                "deko3d device and graphics queue finalized",
                MonotonicClock::now(),
                startedAt );
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
    logger.detach( doggoDevSink );
    doggoDevSink.finalize();
    return exitCode;
  }
}  // namespace doggo::platform::nx
