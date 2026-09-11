#include "doggo/gpu/deko/deko_GraphicsContext.hpp"

#include "doggo/log/log_Log.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace
{
  [[nodiscard]] const char * getDekoResultName( const DkResult result ) noexcept
  {
    switch ( result )
    {
      case DkResult_Success:
        return "Success";

      case DkResult_Fail:
        return "Failure";

      case DkResult_Timeout:
        return "Timeout";

      case DkResult_OutOfMemory:
        return "Out of Memory";

      case DkResult_NotImplemented:
        return "Not Implemented";

      case DkResult_MisalignedSize:
        return "Misaligned Size";

      case DkResult_MisalignedData:
        return "Misaligned Data";

      case DkResult_BadInput:
        return "Bad Input";

      case DkResult_BadFlags:
        return "Bad Flags";

      case DkResult_BadState:
        return "Bad State";
    }

    return "Unknown";
  }
}  // namespace

namespace doggo::gpu::deko
{
  const char * getGraphicsContextStatusName( const GraphicsContextStatus status ) noexcept
  {
    switch ( status )
    {
      case GraphicsContextStatus::Success:
        return "Success";

      case GraphicsContextStatus::DeviceCreationFailed:
        return "Device Creation Failed";

      case GraphicsContextStatus::GraphicsQueueCreationFailed:
        return "Graphics Queue Creation Failed";
    }

    return "Unknown";
  }

  GraphicsContext::~GraphicsContext()
  {
    finalize();
  }

  GraphicsContextStatus GraphicsContext::initialize( log::Logger & logger ) noexcept
  {
    if ( isInitialized() )
    {
      return GraphicsContextStatus::Success;
    }

    finalize();
    mLogger = &logger;

    constexpr std::uint32_t deviceFlags = DkDeviceFlags_DepthZeroToOne | DkDeviceFlags_OriginUpperLeft;
    mDevice                             = dk::DeviceMaker{}
                  .setUserData( this )
                  .setCbDebug( &GraphicsContext::debugCallback )
                  .setFlags( deviceFlags )
                  .create();
    if ( !mDevice )
    {
      mLogger = nullptr;
      return GraphicsContextStatus::DeviceCreationFailed;
    }

    mGraphicsQueue = dk::QueueMaker{ mDevice }.setFlags( DkQueueFlags_Graphics ).create();
    if ( !mGraphicsQueue )
    {
      mDevice = nullptr;
      mLogger = nullptr;
      return GraphicsContextStatus::GraphicsQueueCreationFailed;
    }

    return GraphicsContextStatus::Success;
  }

  void GraphicsContext::finalize() noexcept
  {
    mGraphicsQueue = nullptr;
    mDevice        = nullptr;
    mLogger        = nullptr;
  }

  bool GraphicsContext::isInitialized() const noexcept
  {
    return static_cast<bool>( mDevice ) && static_cast<bool>( mGraphicsQueue );
  }

  dk::Device GraphicsContext::device() const noexcept
  {
    return mDevice;
  }

  dk::Queue GraphicsContext::graphicsQueue() const noexcept
  {
    return mGraphicsQueue;
  }

  void GraphicsContext::debugCallback( void * const       userData,
                                       const char * const callbackContext,
                                       const DkResult     result,
                                       const char * const message ) noexcept
  {
    const auto * const graphicsContext = static_cast<GraphicsContext *>( userData );
    log::Logger *      logger          = graphicsContext ? graphicsContext->mLogger : nullptr;

    if ( logger )
    {
      const char * const safeContext = callbackContext ? callbackContext : "Unknown entrypoint";
      const char * const safeMessage = message ? message : "No diagnostic message";

      std::array<char, log::Logger::FormattedLineCapacity> formattedMessage = {};
      const int                                            written          = std::snprintf( formattedMessage.data(),
                                                                                             formattedMessage.size(),
                                                                                             "%s: %s (%u): %s",
                                                                                             safeContext,
                                                                                             getDekoResultName( result ),
                                                                                             static_cast<unsigned int>( result ),
                                                                                             safeMessage );
      const std::size_t                                    messageSize =
          written <= 0 ? 0 : std::min( static_cast<std::size_t>( written ), formattedMessage.size() - 1 );
      logger->write( result == DkResult_Success ? log::Level::Warning : log::Level::Error,
                     "Deko3D",
                     std::string_view{ formattedMessage.data(), messageSize } );
    }

    if ( result != DkResult_Success )
    {
      if ( logger )
      {
        logger->flush();
      }
      std::abort();
    }
  }
}  // namespace doggo::gpu::deko
