#include "Assert.hpp"

#include "logging/Log.hpp"

namespace doggo::diagnostics
{
    void assertionFailed( std::string_view     expression,
                          std::string_view     message,
                          std::source_location location ) noexcept
    {
        if ( message.empty() )
        {
            logging::critical( "Assertion",
                               "{}:{} in {}: assertion '{}' failed",
                               location.file_name(),
                               location.line(),
                               location.function_name(),
                               expression );
        }
        else
        {
            logging::critical( "Assertion",
                               "{}:{} in {}: assertion '{}' failed: {}",
                               location.file_name(),
                               location.line(),
                               location.function_name(),
                               expression,
                               message );
        }

        std::abort();
    }
} // namespace doggo::diagnostics
