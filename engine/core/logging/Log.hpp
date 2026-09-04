#pragma once

#include <array>
#include <format>

namespace doggo::logging
{
    enum class Level : std::uint8_t
    {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    inline constexpr std::size_t MaxMessageLength = 2048;

    void write( Level level, std::string_view category, std::string_view message ) noexcept;

    template <typename... Args>
    void log( const Level                       level,
              const std::string_view            category,
              const std::format_string<Args...> format,
              Args &&... args )
    {
        std::array<char, MaxMessageLength> buffer = {};
        const auto                         result = std::format_to_n(
            buffer.begin(), static_cast<std::ptrdiff_t>( buffer.size() ), format, std::forward<Args>( args )... );

        const std::size_t messageLength = std::min( static_cast<std::size_t>( result.size ), buffer.size() );
        write( level, category, std::string_view( buffer.data(), messageLength ) );
    }

    template <typename... Args>
    void trace( const std::string_view category, const std::format_string<Args...> format, Args &&... args )
    {
        log( Level::Trace, category, format, std::forward<Args>( args )... );
    }

    template <typename... Args>
    void debug( const std::string_view category, const std::format_string<Args...> format, Args &&... args )
    {
        log( Level::Debug, category, format, std::forward<Args>( args )... );
    }

    template <typename... Args>
    void info( const std::string_view category, const std::format_string<Args...> format, Args &&... args )
    {
        log( Level::Info, category, format, std::forward<Args>( args )... );
    }

    template <typename... Args>
    void warning( const std::string_view category, const std::format_string<Args...> format, Args &&... args )
    {
        log( Level::Warning, category, format, std::forward<Args>( args )... );
    }

    template <typename... Args>
    void error( const std::string_view category, const std::format_string<Args...> format, Args &&... args )
    {
        log( Level::Error, category, format, std::forward<Args>( args )... );
    }

    template <typename... Args>
    void critical( const std::string_view category, const std::format_string<Args...> format, Args &&... args )
    {
        log( Level::Critical, category, format, std::forward<Args>( args )... );
    }
} // namespace doggo::logging
