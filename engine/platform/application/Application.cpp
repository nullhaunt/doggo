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
        constexpr std::size_t BootstrapFrameArenaCapacity = 1024u * 1024u; // 1 MiB
    } // namespace

    int run( platform::ApplicationHost & host, input::Backend & input, render::Backend & renderer )
    {
        logging::info( "Application", "{}", getDescription() );
        logging::info( "Application", "Platform: {}", host.getPlatformName() );
        logging::info( "Application", "Bootstrap OK" );

        if ( !renderer.initialize() )
        {
            logging::critical( "Render", "Renderer initialization failed" );
            return 1;
        }

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

                {
                    profiling::CpuScope renderScope{ cpuProfiler, "Render" };
                    renderer.renderFrame();
                }
            }
        }

        return 0;
    }
} // namespace doggo
