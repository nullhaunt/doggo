#include "Application.hpp"

#include "Doggo.hpp"
#include "logging/Log.hpp"
#include "memory/LinearAllocator.hpp"
#include "profiling/CpuProfiler.hpp"
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
        profiling::CpuProfiler  cpuProfiler;

        while ( host.pumpEvents() )
        {
            frameTimer.tick();
            frameHistory.push( frameTimer.getDeltaTime() );
            frameArena.reset();

            cpuProfiler.beginFrame();
            {
                profiling::CpuScope frameScope{ cpuProfiler, "CPU Frame" };

                {
                    profiling::CpuScope inputScope{ cpuProfiler, "Input" };
                    input.update();
                }

                const input::State & state = input.getState();

                if ( state.wasPressed( input::Button::Start ) )
                {
                    logging::info( "Application", "Exit requested" );
                    break;
                }

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
                                 " Index       : %llu\n"
                                 " Current     : %.3f ms\n"
                                 " Average     : %.3f ms\n"
                                 " Best        : %.3f ms\n"
                                 " Worst       : %.3f ms\n"
                                 " Average FPS : %.2f\n"
                                 "\n",

                                 static_cast<unsigned long long>( frameTimer.getFrameIndex() ),
                                 toMilliseconds( latest ),
                                 toMilliseconds( average ),
                                 toMilliseconds( minimum ),
                                 toMilliseconds( maximum ),
                                 frameHistory.getAverageFps() );

                    std::printf( "CPU (PREVIOUS FRAME)\n" );

                    for ( const profiling::CpuSample & sample : cpuProfiler.getLastFrameSamples() )
                    {
                        const int indentation = static_cast<int>( sample.mDepth * 2 );

                        std::printf( "%*s%-12.*s : %.3f ms\n",
                                     indentation,
                                     "",
                                     static_cast<int>( sample.mName.size() ),
                                     sample.mName.data(),
                                     toMilliseconds( sample.mDuration ) );
                    }

                    if ( cpuProfiler.hasLastFrameOverflowed() )
                    {
                        std::printf( "\nCPU PROFILE OVERFLOW\n" );
                    }

                    std::printf( "\nFRAME MEMORY\n"
                                 " Used        : %zu KiB\n"
                                 " Peak        : %zu KiB\n"
                                 " Capacity    : %zu KiB\n"
                                 "\n\n"

                                 "Press (+) to exit.",

                                 frameArena.getUsed() / 1024u,
                                 frameArena.getPeakUsage() / 1024u,
                                 frameArena.getCapacity() / 1024u );
                }
            }
        }

        return 0;
    }
} // namespace doggo
