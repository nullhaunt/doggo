#pragma once

#include "doggo/doggo_Macro.hpp"

namespace doggo::platform::nx
{
  class Application final
  {
      DOGGO_DISALLOW_COPY( Application );
      DOGGO_DISALLOW_MOVE( Application );

    public:
      Application() = default;

      int run();
  };
}  // namespace doggo::platform::nx