#pragma once

#include "doggo/log/log_Log.hpp"

#include <string_view>

namespace doggo::platform::nx
{
  class ConsoleLogSink final : public log::Sink
  {
    public:
      void write( std::string_view text ) noexcept override;
      void flush() noexcept override;
  };

  class DoggoDevLogSink final : public log::Sink
  {
      DOGGO_DISALLOW_COPY( DoggoDevLogSink );
      DOGGO_DISALLOW_MOVE( DoggoDevLogSink );

    public:
      DoggoDevLogSink() = default;
      ~DoggoDevLogSink() override;

      [[nodiscard]] bool initialize() noexcept;
      void               finalize() noexcept;
      [[nodiscard]] bool isConnected() const noexcept;

      void write( std::string_view text ) noexcept override;

    private:
      int  mSocket              = -1;
      bool mIsSocketInitialized = false;
  };
}  // namespace doggo::platform::nx
