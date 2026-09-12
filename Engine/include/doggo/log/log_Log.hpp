#pragma once

#include "doggo/doggo_Macro.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace doggo::log
{
  enum class Level : std::uint8_t
  {
    Trace,
    Info,
    Warning,
    Error,
  };

  struct Record final
  {
      static constexpr std::size_t CategoryCapacity = 31;
      static constexpr std::size_t MessageCapacity  = 383;

      std::uint64_t                          sequence         = 0;
      std::chrono::nanoseconds               elapsed          = std::chrono::nanoseconds::zero();
      Level                                  level            = Level::Info;
      std::array<char, CategoryCapacity + 1> category_storage = {};
      std::array<char, MessageCapacity + 1>  message_storage  = {};
      std::uint16_t                          category_size    = 0;
      std::uint16_t                          message_size     = 0;
      bool                                   was_truncated    = false;

      [[nodiscard]] std::string_view category() const noexcept;
      [[nodiscard]] std::string_view message() const noexcept;
  };

  class Sink
  {
      DOGGO_DISALLOW_COPY( Sink );
      DOGGO_DISALLOW_MOVE( Sink );

    public:
      Sink()          = default;
      virtual ~Sink() = default;

      virtual void write( std::string_view text ) noexcept = 0;
      virtual void flush() noexcept;
  };

  // Record storage and fan-out use a bounded ring so writes never allocate.
  // The logger remains synchronous until the runtime introduces its threading
  // and diagnostics ownership model.
  class Logger final
  {
      DOGGO_DISALLOW_COPY( Logger );
      DOGGO_DISALLOW_MOVE( Logger );

    public:
      static constexpr std::size_t SinkCapacity          = 4;
      static constexpr std::size_t RecordCapacity        = 128;
      static constexpr std::size_t FormattedLineCapacity = 512;

      Logger() = default;

      [[nodiscard]] bool attach( Sink & sink ) noexcept;
      void               detach( Sink & sink ) noexcept;

      void write( Level                    level,
                  std::string_view         category,
                  std::string_view         message,
                  std::chrono::nanoseconds elapsed = std::chrono::nanoseconds::zero() ) noexcept;

      void flush() noexcept;
      void clear() noexcept;

      [[nodiscard]] std::size_t    size() const noexcept;
      [[nodiscard]] const Record * recordAt( std::size_t logicalIndex ) const noexcept;

    private:
      std::array<Sink *, SinkCapacity>   mSinks        = {};
      std::array<Record, RecordCapacity> mRecords      = {};
      std::size_t                        mSinkCount    = 0;
      std::size_t                        mRecordCount  = 0;
      std::size_t                        mNextRecord   = 0;
      std::uint64_t                      mNextSequence = 1;
  };

  [[nodiscard]] std::string_view levelName( Level level ) noexcept;
}  // namespace doggo::log
