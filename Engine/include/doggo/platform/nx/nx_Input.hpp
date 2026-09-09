#pragma once

#include "doggo/doggo_Macro.hpp"

#include <switch.h>

#include <cstdint>

namespace doggo::platform::nx
{
  struct AnalogStickPosition final
  {
      std::int32_t x = 0;
      std::int32_t y = 0;
  };

  struct InputSnapshot final
  {
      bool                is_connected = false;
      bool                is_handheld  = false;
      std::uint32_t       style_set    = 0;
      std::uint32_t       attributes   = 0;
      std::uint64_t       buttons_held = 0;
      std::uint64_t       buttons_down = 0;
      std::uint64_t       buttons_up   = 0;
      AnalogStickPosition left_stick;
      AnalogStickPosition right_stick;
  };

  class Input final
  {
      DOGGO_DISALLOW_COPY( Input );
      DOGGO_DISALLOW_MOVE( Input );

    public:
      Input() noexcept = default;

      void                                initialize() noexcept;
      [[nodiscard]] const InputSnapshot & update() noexcept;

    private:
      PadState      mPad           = {};
      InputSnapshot mSnapshot      = {};
      bool          mIsInitialized = false;
  };
}  // namespace doggo::platform::nx
