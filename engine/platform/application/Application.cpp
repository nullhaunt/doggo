#include "Application.hpp"

#include "Doggo.hpp"
#include "diagnostics/Assert.hpp"
#include "logging/Log.hpp"
#include "memory/LinearAllocator.hpp"
#include "time/FrameTimer.hpp"

namespace doggo
{
    namespace
    {
        constexpr std::size_t BootstrapFrameArenaCapacity = 1024u * 1024u; // 1 MiB
    }

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

        time::FrameTimer frameTimer;
        while ( host.pumpEvents() )
        {
            frameTimer.tick();
            frameArena.reset();
            input.update();

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
