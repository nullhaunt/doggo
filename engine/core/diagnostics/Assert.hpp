#pragma once

#include <source_location>
#include <string_view>

namespace doggo::diagnostics
{
    [[noreturn]] void assertionFailed( std::string_view expression,
                                       std::string_view message,
                                       std::source_location ) noexcept;
}

#if !defined( NDEBUG )
    #define DOGGO_ASSERT( condition )                                                                                  \
        do                                                                                                             \
        {                                                                                                              \
            if ( !( condition ) )                                                                                      \
            {                                                                                                          \
                ::doggo::diagnostics::assertionFailed( #condition, {}, std::source_location::current() );              \
            }                                                                                                          \
        } while ( false )

    #define DOGGO_ASSERT_MSG( condition, message )                                                                     \
        do                                                                                                             \
        {                                                                                                              \
            if ( !( condition ) )                                                                                      \
            {                                                                                                          \
                ::doggo::diagnostics::assertionFailed( #condition, message, std::source_location::current() );         \
            }                                                                                                          \
        } while ( false )
#else
    #define DOGGO_ASSERT( condition )              ( ( void )0 )
    #define DOGGO_ASSERT_MSG( condition, message ) ( ( void )0 )
#endif
