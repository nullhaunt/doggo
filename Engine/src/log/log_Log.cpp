#include "doggo/log/log_Log.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace
{
  template <std::size_t Capacity>
  [[nodiscard]] std::uint16_t copyText( std::array<char, Capacity> & destination,
                                        const std::string_view       source ) noexcept
  {
    static_assert( Capacity > 0 );

    const std::size_t copiedSize = std::min( source.size(), Capacity - 1 );
    if ( copiedSize != 0 )
    {
      std::memcpy( destination.data(), source.data(), copiedSize );
    }
    destination[ copiedSize ] = '\0';
    return static_cast<std::uint16_t>( copiedSize );
  }

  [[nodiscard]] std::string_view
  formatRecord( const doggo::log::Record &                                    record,
                std::array<char, doggo::log::Logger::FormattedLineCapacity> & output ) noexcept
  {
    const auto elapsedMicroseconds =
        std::max<std::int64_t>( 0, std::chrono::duration_cast<std::chrono::microseconds>( record.elapsed ).count() );
    const auto elapsedMilliseconds = elapsedMicroseconds / 1'000;
    const auto elapsedFraction     = elapsedMicroseconds % 1'000;
    const auto category            = record.category();

    const auto [ outIt, totalSize ] = std::format_to_n( output.data(),
                                                        output.size(),
                                                        "[+{}.{:03d} ms] [{:^7}] [{:.{}}] ",
                                                        static_cast<long long>( elapsedMilliseconds ),
                                                        static_cast<long long>( elapsedFraction ),
                                                        doggo::log::levelName( record.level ).data(),
                                                        category,
                                                        category.size() );

    const int prefixSize = static_cast<int>( totalSize );
    if ( prefixSize < 0 )
    {
      return {};
    }

    std::size_t cursor = std::min<std::size_t>( static_cast<std::size_t>( prefixSize ), output.size() - 1 );
    const auto  append = [ &output, &cursor ]( const std::string_view text ) noexcept
    {
      const std::size_t available = output.size() - 1 - cursor;
      const std::size_t copySize  = std::min( available, text.size() );
      if ( copySize != 0 )
      {
        std::memcpy( output.data() + cursor, text.data(), copySize );
        cursor += copySize;
      }
    };

    append( record.message() );
    if ( record.was_truncated )
    {
      append( " [truncated]" );
    }
    append( "\n" );
    output[ cursor ] = '\0';
    return { output.data(), cursor };
  }
}  // namespace

namespace doggo::log
{
  std::string_view Record::category() const noexcept
  {
    return { category_storage.data(), category_size };
  }

  std::string_view Record::message() const noexcept
  {
    return { message_storage.data(), message_size };
  }

  void Sink::flush() noexcept
  {
  }

  bool Logger::attach( Sink & sink ) noexcept
  {
    for ( std::size_t index = 0; index < mSinkCount; ++index )
    {
      if ( mSinks[ index ] == &sink )
      {
        return true;
      }
    }

    if ( mSinkCount == mSinks.size() )
    {
      return false;
    }

    mSinks[ mSinkCount++ ] = &sink;
    return true;
  }

  void Logger::detach( Sink & sink ) noexcept
  {
    for ( std::size_t index = 0; index < mSinkCount; ++index )
    {
      if ( mSinks[ index ] != &sink )
      {
        continue;
      }

      for ( std::size_t moveIndex = index + 1; moveIndex < mSinkCount; ++moveIndex )
      {
        mSinks[ moveIndex - 1 ] = mSinks[ moveIndex ];
      }
      mSinks[ --mSinkCount ] = nullptr;
      return;
    }
  }

  void Logger::write( const Level                    level,
                      const std::string_view         category,
                      const std::string_view         message,
                      const std::chrono::nanoseconds elapsed ) noexcept
  {
    Record & record = mRecords[ mNextRecord ];
    record.sequence = mNextSequence++;
    record.elapsed  = std::max( elapsed, std::chrono::nanoseconds::zero() );
    record.level    = level;

    record.category_size = copyText( record.category_storage, category );
    record.message_size  = copyText( record.message_storage, message );
    record.was_truncated = category.size() > Record::CategoryCapacity || message.size() > Record::MessageCapacity;

    mNextRecord  = ( mNextRecord + 1 ) % mRecords.size();
    mRecordCount = std::min( mRecordCount + 1, mRecords.size() );

    std::array<char, FormattedLineCapacity> formattedLine = {};
    const std::string_view                  text          = formatRecord( record, formattedLine );
    for ( std::size_t index = 0; index < mSinkCount; ++index )
    {
      mSinks[ index ]->write( text );
    }
  }

  void Logger::flush() noexcept
  {
    for ( std::size_t index = 0; index < mSinkCount; ++index )
    {
      mSinks[ index ]->flush();
    }
  }

  void Logger::clear() noexcept
  {
    mRecordCount  = 0;
    mNextRecord   = 0;
    mNextSequence = 1;
  }

  std::size_t Logger::size() const noexcept
  {
    return mRecordCount;
  }

  const Record * Logger::recordAt( const std::size_t logicalIndex ) const noexcept
  {
    if ( logicalIndex >= mRecordCount )
    {
      return nullptr;
    }

    const std::size_t oldestRecord = ( mNextRecord + mRecords.size() - mRecordCount ) % mRecords.size();
    return &mRecords[ ( oldestRecord + logicalIndex ) % mRecords.size() ];
  }

  std::string_view levelName( const Level level ) noexcept
  {
    switch ( level )
    {
      case Level::Trace:
        return "TRACE";

      case Level::Info:
        return "INFO";

      case Level::Warning:
        return "WARN";

      case Level::Error:
        return "ERROR";
    }

    return "UNKNOWN";
  }
}  // namespace doggo::log
