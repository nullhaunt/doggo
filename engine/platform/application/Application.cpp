#include "Application.hpp"

#include "Doggo.hpp"
#include "diagnostics/Assert.hpp"
#include "logging/Log.hpp"
#include "memory/LinearAllocator.hpp"
#include "profiling/FrameHistory.hpp"
#include "time/FrameTimer.hpp"

namespace doggo
{
    namespace
    {
        constexpr std::size_t   BootstrapFrameArenaCapacity = 1024u * 1024u; // 1 MiB
        constexpr std::uint64_t TelemetryRefreshFrames      = 15;

        [[nodiscard]] double toMilliseconds( const std::chrono::nanoseconds duration ) noexcept
        {
            return std::chrono::duration<double, std::milli>( duration ).count();
        }
    } // namespace

    int run( platform::ApplicationHost & host, input::Backend & input )
    {
        logging::info( "Application", "{}", getDescription() );
        logging::info( "Application", "Platform: {}", host.getPlatformName() );
        logging::info( "Application", "Bootstrap OK" );

        std::unique_ptr<std::byte[]> frameMemory{ new ( std::nothrow ) std::byte[ BootstrapFrameArenaCapacity ] };
        if ( !frameMemory )
        {
            logging::critical(
                "Memory", "Failed to allocate {} bytes for the frame arena", BootstrapFrameArenaCapacity );
            return 1;
        }

        memory::LinearAllocator frameArena{ std::span{ frameMemory.get(), BootstrapFrameArenaCapacity } };
        logging::info( "Memory", "Frame arena capacity: {} KiB", frameArena.getCapacity() / 1024u );

        time::FrameTimer        frameTimer;
        profiling::FrameHistory frameHistory;

        while ( host.pumpEvents() )
        {
            frameTimer.tick();
            frameHistory.push( frameTimer.getDeltaTime() );
            frameArena.reset();

            input.update();

            if ( frameTimer.getFrameIndex() % TelemetryRefreshFrames == 0 )
            {
                const auto latest  = frameHistory.getLatest();
                const auto average = frameHistory.getAverage();
                const auto minimum = frameHistory.getMinimum();
                const auto maximum = frameHistory.getMaximum();

                std::printf( "\x1b[2J"
                             "\x1b[H"
                             "DOGGO\n"
                             "Data-Oriented Geometry & Gameplay Orchestrator\n"
                             "\n"

                             "FRAME\n"
                             "\tIndex       : %llu\n"
                             "\tCurrent     : %.3f ms\n"
                             "\tAverage     : %.3f ms\n"
                             "\tBest        : %.3f ms\n"
                             "\tWorst       : %.3f ms\n"
                             "\tAverage FPS : %.2f\n"
                             "\n"

                             "FRAME MEMORY\n"
                             "\tUsed        : %zu KiB\n"
                             "\tPeak        : %zu KiB\n"
                             "\tCapacity    : %zu KiB\n"
                             "\n\n"

                             "Press (+) to exit.",

                             static_cast<unsigned long long>( frameTimer.getFrameIndex() ),
                             toMilliseconds( latest ),
                             toMilliseconds( average ),
                             toMilliseconds( minimum ),
                             toMilliseconds( maximum ),
                             frameHistory.getAverageFps(),

                             frameArena.getUsed() / 1024u,
                             frameArena.getPeakUsage() / 1024u,
                             frameArena.getCapacity() / 1024u );
            }

            const input::State & state = input.getState();

            if ( state.wasPressed( input::Button::Start ) )
            {
                logging::info( "Application", "Exit requested" );
                break;
            }
        }

        return 0;
    }
} // namespace doggo
