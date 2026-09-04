#include "Log.hpp"

namespace doggo::logging
{
    namespace
    {
        [[nodiscard]] constexpr std::string_view getLevelName( const Level level ) noexcept
        {
            switch ( level )
            {
                case Level::Trace:
                    return "TRACE";

                case Level::Debug:
                    return "DEBUG";

                case Level::Info:
                    return "INFO";

                case Level::Warning:
                    return "WARN";

                case Level::Error:
                    return "ERROR";

                case Level::Critical:
                    return "CRITICAL";
            }

            return "UNKNOWN";
        }
    } // namespace

    void write( Level level, std::string_view category, std::string_view message ) noexcept
    {
        const std::string_view levelName = getLevelName( level );

        std::fprintf( stdout,
                      "[DOGGO] [%.*s] [%.*s] %.*s\n",
                      static_cast<int>( levelName.size() ),
                      levelName.data(),
                      static_cast<int>( category.size() ),
                      category.data(),
                      static_cast<int>( message.size() ),
                      message.data() );

        std::fflush( stdout );
    }
} // namespace doggo::logging
