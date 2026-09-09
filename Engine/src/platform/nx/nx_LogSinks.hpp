#pragma once

#include "doggo/log/Log.hpp"

#include <string_view>

namespace doggo::platform::nx
{
  class ConsoleLogSink final : public log::Sink
  {
    public:
      void write( std::string_view text ) noexcept override;
      void flush() noexcept override;
  };

  class NxlinkLogSink final : public log::Sink
  {
      DOGGO_DISALLOW_COPY( NxlinkLogSink );
      DOGGO_DISALLOW_MOVE( NxlinkLogSink );

    public:
      NxlinkLogSink() = default;
      ~NxlinkLogSink() override;

      [[nodiscard]] bool initialize() noexcept;
      void               finalize() noexcept;
      [[nodiscard]] bool isConnected() const noexcept;

      void write( std::string_view text ) noexcept override;

    private:
      int  mSocket              = -1;
      bool mIsSocketInitialized = false;
  };
}  // namespace doggo::platform::nx
