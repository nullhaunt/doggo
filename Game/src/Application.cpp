#include "Application.hpp"

#include <doggo/gpu/deko/deko_GraphicsContext.hpp>
#include <doggo/gpu/deko/deko_GraphicsProgram.hpp>
#include <doggo/gpu/deko/deko_MemoryArena.hpp>
#include <doggo/gpu/deko/deko_Presenter.hpp>
#include <doggo/log/log_Log.hpp>
#include <doggo/platform/nx/nx_AppletLifecycle.hpp>
#include <doggo/platform/nx/nx_Input.hpp>
#include <doggo/platform/nx/nx_LogSinks.hpp>
#include <doggo/platform/nx/nx_Memory.hpp>
#include <doggo/platform/nx/nx_MonotonicClock.hpp>
#include <doggo/platform/nx/nx_RomFs.hpp>

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
  constexpr double        BytesPerMebibyte               = 1024.0 * 1024.0;
  constexpr char          BootstrapVertexShaderPath[]    = "romfs:/shaders/bootstrap/doggo_bootstrap_triangle_vsh.dksh";
  constexpr char          BootstrapFragmentShaderPath[]  = "romfs:/shaders/bootstrap/doggo_bootstrap_triangle_fsh.dksh";
  constexpr std::size_t   BootstrapShaderBinaryCapacity  = 64u * 1024u;
  constexpr std::uint32_t ShaderCodeArenaMemoryBlockSize = 4u * 1024u * 1024u;
  constexpr std::uint32_t HandheldWidth                  = 1280;
  constexpr std::uint32_t HandheldHeight                 = 720;
  constexpr std::uint32_t DockedWidth                    = 1920;
  constexpr std::uint32_t DockedHeight                   = 1080;

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
                 const doggo::platform::nx::MonotonicClock::time_point startedAt ) noexcept
  {
    logger.write( level, category, message, elapsedSince( doggo::platform::nx::MonotonicClock::now(), startedAt ) );
  }

  [[nodiscard]] std::string formatResult( const std::uint32_t result )
  {
    return std::format( "0x{:08X}", result );
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
        return "Destination Too Small";

      case Status::ReadFailed:
        return "Read Failed";

      case Status::CloseFailed:
        return "Close Failed";
    }

    return "Unknown";
  }

  [[nodiscard]] doggo::gpu::deko::PresentationExtent
  getPresentationExtent( const AppletOperationMode operationMode ) noexcept
  {
    if ( operationMode == AppletOperationMode_Console )
    {
      return { .width = DockedWidth, .height = DockedHeight };
    }
    return { .width = HandheldWidth, .height = HandheldHeight };
  }

  [[nodiscard]] doggo::gpu::deko::PresentationReport
  renderBootstrapFrame( doggo::gpu::deko::Presenter &             presenter,
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
                "Asset",
                std::format( "Shader read failed for {}: {} (errno {}, {} of {} bytes)",
                             path,
                             getRomFsReadStatusName( report.status ),
                             report.error_number,
                             report.bytes_read,
                             report.file_size ),
                startedAt );
      return false;
    }

    if ( report.bytes_read == 0 )
    {
      writeLog( logger, doggo::log::Level::Error, "Asset", std::format( "Shader is empty: {}", path ), startedAt );
      return false;
    }

    binarySize = report.bytes_read;
    return true;
  }

  [[nodiscard]] bool initializeBootstrapProgram( doggo::log::Logger &                                  logger,
                                                 const doggo::platform::nx::RomFs &                    romFs,
                                                 doggo::gpu::deko::GraphicsProgram &                   graphicsProgram,
                                                 doggo::gpu::deko::MemoryArena &                       shaderCodeArena,
                                                 const doggo::platform::nx::MonotonicClock::time_point startedAt )
  {
    std::array<std::uint8_t, BootstrapShaderBinaryCapacity> vertexBinary   = {};
    std::array<std::uint8_t, BootstrapShaderBinaryCapacity> fragmentBinary = {};
    std::size_t                                             vertexSize     = 0;
    std::size_t                                             fragmentSize   = 0;

    if ( !readShaderBinary( logger, romFs, BootstrapVertexShaderPath, vertexBinary, vertexSize, startedAt ) ||
         !readShaderBinary( logger, romFs, BootstrapFragmentShaderPath, fragmentBinary, fragmentSize, startedAt ) )
    {
      return false;
    }

    const doggo::gpu::deko::GraphicsProgramStatus status =
        graphicsProgram.initialize( shaderCodeArena,
                                    std::span<const std::uint8_t>{ vertexBinary.data(), vertexSize },
                                    std::span<const std::uint8_t>{ fragmentBinary.data(), fragmentSize } );
    if ( status == doggo::gpu::deko::GraphicsProgramStatus::Success )
    {
      return true;
    }

    writeLog( logger,
              doggo::log::Level::Error,
              "GPU",
              std::format( "Bootstrap graphics program initialization failed: {}",
                           doggo::gpu::deko::getGraphicsProgramStatusName( status ) ),
              startedAt );
    return false;
  }
}  // namespace

namespace doggo::game
{
  int Application::run()
  {
    using platform::nx::MonotonicClock;

    const MonotonicClock::time_point startedAt = MonotonicClock::now();
    log::Logger                      logger;
    platform::nx::DoggoDevLogSink    doggoDevSink;
    const bool                       isRemoteLogAttached = doggoDevSink.initialize() && logger.attach( doggoDevSink );
    if ( !isRemoteLogAttached )
    {
      doggoDevSink.finalize();
    }

    int exitCode = EXIT_SUCCESS;
    writeLog( logger, log::Level::Info, "Startup", "DOGGO", startedAt );

    platform::nx::AppletLifecycle lifecycle;
    const std::uint32_t           lifecycleResult = lifecycle.initialize();
    if ( R_FAILED( lifecycleResult ) )
    {
      writeLog( logger,
                log::Level::Error,
                "Lifecycle",
                std::format( "Hook initialization failed: {}", formatResult( lifecycleResult ) ),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    platform::nx::Input input;
    input.initialize();

    gpu::deko::GraphicsContext             graphicsContext;
    const gpu::deko::GraphicsContextStatus graphicsStatus = graphicsContext.initialize( logger );
    if ( graphicsStatus != gpu::deko::GraphicsContextStatus::Success )
    {
      writeLog( logger,
                log::Level::Error,
                "GPU",
                std::format( "Initialization failed: {}", gpu::deko::getGraphicsContextStatusName( graphicsStatus ) ),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    gpu::deko::Presenter          presenter;
    gpu::deko::PresentationReport presentationReport = {
        .status = gpu::deko::PresentationStatus::NotInitialized,
    };
    const gpu::deko::PresentationExtent initialExtent = getPresentationExtent( appletGetOperationMode() );
    if ( graphicsStatus == gpu::deko::GraphicsContextStatus::Success )
    {
      presentationReport = presenter.initialize( graphicsContext.device(),
                                                 graphicsContext.graphicsQueue(),
                                                 nwindowGetDefault(),
                                                 initialExtent );
    }
    if ( presentationReport.status != gpu::deko::PresentationStatus::Success )
    {
      writeLog( logger,
                log::Level::Error,
                "GPU",
                std::format( "Presentation initialization failed: {} (deko result {}, context {})",
                             gpu::deko::getPresentationStatusName( presentationReport.status ),
                             static_cast<std::uint32_t>( presentationReport.deko_result ),
                             presentationReport.context_index ),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    platform::nx::RomFs romFs;
    const std::uint32_t romFsResult = romFs.initialize();
    if ( R_FAILED( romFsResult ) )
    {
      writeLog( logger,
                log::Level::Error,
                "Asset",
                std::format( "ROMFS mount failed: {}", formatResult( romFsResult ) ),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    gpu::deko::MemoryArena shaderCodeArena;
    if ( graphicsContext.isInitialized() )
    {
      constexpr gpu::deko::MemoryArenaConfig shaderCodeArenaConfig = {
          .memory_block_size_bytes = ShaderCodeArenaMemoryBlockSize,
          .reserved_tail_bytes     = DK_SHADER_CODE_UNUSABLE_SIZE,
          .flags                   = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code,
          .release_policy          = gpu::deko::MemoryArenaReleasePolicy::RetainedUntilFinalize,
          .require_cpu_mapping     = true,
      };
      const gpu::deko::MemoryArenaStatus shaderCodeArenaStatus =
          shaderCodeArena.initialize( graphicsContext.device(), shaderCodeArenaConfig );
      if ( shaderCodeArenaStatus != gpu::deko::MemoryArenaStatus::Success )
      {
        writeLog( logger,
                  log::Level::Error,
                  "GPU",
                  std::format( "Shader-code arena initialization failed: {}",
                               gpu::deko::getMemoryArenaStatusName( shaderCodeArenaStatus ) ),
                  startedAt );
        exitCode = EXIT_FAILURE;
      }
    }

    gpu::deko::GraphicsProgram bootstrapProgram;
    if ( romFs.isInitialized() &&
         shaderCodeArena.isInitialized() &&
         !initializeBootstrapProgram( logger, romFs, bootstrapProgram, shaderCodeArena, startedAt ) )
    {
      exitCode = EXIT_FAILURE;
    }

    platform::nx::MemoryReport memoryReport;
    const std::uint32_t        memoryResult = platform::nx::queryMemoryReport( memoryReport );
    if ( R_FAILED( memoryResult ) )
    {
      writeLog( logger,
                log::Level::Error,
                "Memory",
                std::format( "Query failed: {}", formatResult( memoryResult ) ),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    if ( exitCode == EXIT_SUCCESS )
    {
      const gpu::deko::MemoryArenaSnapshot shaderArenaSnapshot = shaderCodeArena.snapshot();
      writeLog( logger,
                log::Level::Info,
                "Startup",
                std::format( "Ready: {}x{}, {} frame contexts, {:.2f} MiB free, shader code {} / {} KiB",
                             initialExtent.width,
                             initialExtent.height,
                             gpu::deko::Presenter::FrameCount,
                             static_cast<double>( memoryReport.process_free_bytes ) / BytesPerMebibyte,
                             shaderArenaSnapshot.allocations.used_bytes / 1024u,
                             shaderArenaSnapshot.allocations.capacity_bytes / 1024u ),
                startedAt );
    }

    bool                          isRunning       = exitCode == EXIT_SUCCESS;
    AppletFocusState              focusState      = appletGetFocusState();
    gpu::deko::PresentationExtent requestedExtent = initialExtent;
    MonotonicClock::time_point    previousTime    = startedAt;

    while ( isRunning && appletMainLoop() )
    {
      platform::nx::AppletLifecycleEvent lifecycleEvent;
      while ( lifecycle.tryPopEvent( lifecycleEvent ) )
      {
        switch ( lifecycleEvent.type )
        {
          case platform::nx::AppletLifecycleEventType::FocusStateChanged:
            focusState = static_cast<AppletFocusState>( lifecycleEvent.detail );
            break;

          case platform::nx::AppletLifecycleEventType::OperationModeChanged:
            requestedExtent = getPresentationExtent( static_cast<AppletOperationMode>( lifecycleEvent.detail ) );
            break;

          case platform::nx::AppletLifecycleEventType::Resumed:
            requestedExtent = getPresentationExtent( appletGetOperationMode() );
            break;

          case platform::nx::AppletLifecycleEventType::ExitRequested:
            isRunning = false;
            break;

          case platform::nx::AppletLifecycleEventType::PerformanceModeChanged:
            break;
        }
      }

      const MonotonicClock::time_point currentTime = MonotonicClock::now();
      if ( currentTime < previousTime )
      {
        writeLog( logger, log::Level::Error, "Timing", "Monotonic clock moved backwards", startedAt );
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
        const Result waitResult = eventWait( appletGetMessageEvent(), std::numeric_limits<std::uint64_t>::max() );
        if ( R_FAILED( waitResult ) )
        {
          writeLog( logger,
                    log::Level::Error,
                    "Lifecycle",
                    std::format( "Wait failed: {}", formatResult( waitResult ) ),
                    startedAt );
          exitCode = EXIT_FAILURE;
          break;
        }
        continue;
      }

      if ( requestedExtent != presenter.extent() )
      {
        const gpu::deko::PresentationExtent previousExtent = presenter.extent();
        const gpu::deko::PresentationReport resizeReport   = presenter.resize( requestedExtent );
        if ( resizeReport.status != gpu::deko::PresentationStatus::Success )
        {
          writeLog( logger,
                    log::Level::Error,
                    "GPU",
                    std::format( "Presentation resize failed: {} (deko result {}, context {})",
                                 gpu::deko::getPresentationStatusName( resizeReport.status ),
                                 static_cast<std::uint32_t>( resizeReport.deko_result ),
                                 resizeReport.context_index ),
                    startedAt );
          exitCode = EXIT_FAILURE;
          break;
        }

        writeLog( logger,
                  log::Level::Info,
                  "GPU",
                  std::format( "Presentation resized: {}x{} -> {}x{}",
                               previousExtent.width,
                               previousExtent.height,
                               requestedExtent.width,
                               requestedExtent.height ),
                  startedAt );
      }

      const platform::nx::InputSnapshot & inputSnapshot = input.update();
      if ( ( inputSnapshot.buttons_down & HidNpadButton_Plus ) != 0 )
      {
        break;
      }

      const gpu::deko::PresentationReport frameReport = renderBootstrapFrame( presenter, bootstrapProgram );
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
                  startedAt );
        exitCode = EXIT_FAILURE;
        break;
      }
    }

    if ( lifecycle.droppedEventCount() != 0 )
    {
      writeLog( logger,
                log::Level::Error,
                "Lifecycle",
                std::format( "Dropped {} lifecycle events", lifecycle.droppedEventCount() ),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    const gpu::deko::PresentationReport finalizeReport = presenter.finalize();
    if ( finalizeReport.status != gpu::deko::PresentationStatus::Success )
    {
      writeLog( logger,
                log::Level::Error,
                "GPU",
                std::format( "Presentation finalization failed: {} (deko result {}, context {})",
                             gpu::deko::getPresentationStatusName( finalizeReport.status ),
                             static_cast<std::uint32_t>( finalizeReport.deko_result ),
                             finalizeReport.context_index ),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    bootstrapProgram.finalize();
    shaderCodeArena.finalize();
    graphicsContext.finalize();

    const std::uint32_t romFsFinalizeResult = romFs.finalize();
    if ( R_FAILED( romFsFinalizeResult ) )
    {
      writeLog( logger,
                log::Level::Error,
                "Asset",
                std::format( "ROMFS unmount failed: {}", formatResult( romFsFinalizeResult ) ),
                startedAt );
      exitCode = EXIT_FAILURE;
    }

    lifecycle.finalize();
    writeLog( logger,
              log::Level::Info,
              "Shutdown",
              std::format( "Application exiting with code {}", exitCode ),
              startedAt );
    logger.flush();

    if ( isRemoteLogAttached )
    {
      logger.detach( doggoDevSink );
    }

    doggoDevSink.finalize();
    return exitCode;
  }
}  // namespace doggo::game
