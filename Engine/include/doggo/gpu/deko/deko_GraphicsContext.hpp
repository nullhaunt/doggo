#pragma once

#include "doggo/doggo_Macro.hpp"

#include <deko3d.hpp>

#include <cstdint>

namespace doggo::log
{
  class Logger;
}

namespace doggo::gpu::deko
{
  enum class GraphicsContextStatus : std::uint8_t
  {
    Success,
    DeviceCreationFailed,
    GraphicsQueueCreationFailed,
  };

  [[nodiscard]] const char * getGraphicsContextStatusName( GraphicsContextStatus status ) noexcept;

  class GraphicsContext final
  {
      DOGGO_DISALLOW_COPY( GraphicsContext );
      DOGGO_DISALLOW_MOVE( GraphicsContext );

    public:
      GraphicsContext() noexcept = default;
      ~GraphicsContext();

      [[nodiscard]] GraphicsContextStatus initialize( log::Logger & logger ) noexcept;
      void                                finalize() noexcept;

      [[nodiscard]] bool       isInitialized() const noexcept;
      [[nodiscard]] dk::Device device() const noexcept;
      [[nodiscard]] dk::Queue  graphicsQueue() const noexcept;

    private:
      static void debugCallback( void * userData, const char * callbackContext, DkResult result,
                                 const char * message ) noexcept;

      log::Logger *    mLogger = nullptr;
      dk::UniqueDevice mDevice;
      dk::UniqueQueue  mGraphicsQueue;
  };
}  // namespace doggo::gpu::deko
