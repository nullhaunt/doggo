#include "Application.hpp"

#include "Doggo.hpp"
#include "diagnostics/Assert.hpp"
#include "logging/Log.hpp"
#include "memory/LinearAllocator.hpp"
#include "time/FrameTimer.hpp"

namespace doggo
{
    int run( platform::ApplicationHost & host, input::Backend & input )
    {
        logging::info( "Application", "{}", getDescription() );
        logging::info( "Application", "Platform: {}", host.getPlatformName() );
        logging::info( "Application", "Bootstrap OK" );

        std::array<std::byte, 256> storage = {};
        memory::LinearAllocator    allocator{ std::span<std::byte>{ storage } };

        DOGGO_ASSERT( allocator.getCapacity() == storage.size() );
        DOGGO_ASSERT( allocator.getUsed() == 0 );

        void * first = allocator.allocate( 3, 1 );
        DOGGO_ASSERT( first );

        void * aligned = allocator.allocate( 16, 16 );
        DOGGO_ASSERT( aligned );
        DOGGO_ASSERT( reinterpret_cast<std::uintptr_t>( aligned ) % 16 == 0 );

        const std::size_t usedBeforeFailure = allocator.getUsed();

        void * tooLarge = allocator.allocate( 1024, 16 );
        DOGGO_ASSERT( !tooLarge );

        DOGGO_ASSERT( allocator.getUsed() == usedBeforeFailure );

        const std::size_t peak = allocator.getPeakUsage();

        allocator.reset();
        DOGGO_ASSERT( allocator.getUsed() == 0 );

        DOGGO_ASSERT( allocator.getPeakUsage() == peak );

        time::FrameTimer frameTimer;
        while ( host.pumpEvents() )
        {
            input.update();
            frameTimer.tick();

            const input::State & state = input.getState();

            if ( state.wasPressed( input::Button::Start ) )
            {
                logging::info( "Application", "Exit requested" );
                break;
            }

            if ( state.wasPressed( input::Button::Back ) )
            {
                DOGGO_ASSERT( false );
            }
        }

        return 0;
    }
} // namespace doggo
