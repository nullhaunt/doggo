#include "doggo/platform/nx/nx_RomFs.hpp"

#include <switch.h>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace doggo::platform::nx
{
  RomFs::~RomFs()
  {
    static_cast<void>( finalize() );
  }

  std::uint32_t RomFs::initialize() noexcept
  {
    if ( mIsInitialized )
    {
      return 0;
    }

    const Result result = romfsInit();
    if ( R_SUCCEEDED( result ) )
    {
      mIsInitialized = true;
    }

    return result;
  }

  std::uint32_t RomFs::finalize() noexcept
  {
    if ( !mIsInitialized )
    {
      return 0;
    }

    const Result result = romfsExit();
    if ( R_SUCCEEDED( result ) )
    {
      mIsInitialized = false;
    }

    return result;
  }

  RomFsReadReport RomFs::readFile( const char * const path, const std::span<std::uint8_t> destination ) const noexcept
  {
    RomFsReadReport report;
    if ( !mIsInitialized )
    {
      return report;
    }

    if ( path == nullptr || path[ 0 ] == '\0' )
    {
      report.status       = RomFsReadStatus::InvalidArgument;
      report.error_number = EINVAL;
      return report;
    }

    report.status = RomFsReadStatus::Success;

    const int file = ::open( path, O_RDONLY );
    if ( file < 0 )
    {
      report.status       = RomFsReadStatus::OpenFailed;
      report.error_number = errno;
      return report;
    }

    struct stat fileStatus = {};
    if ( ::fstat( file, &fileStatus ) != 0 )
    {
      report.status       = RomFsReadStatus::StatFailed;
      report.error_number = errno;
      static_cast<void>( ::close( file ) );
      return report;
    }

    if ( fileStatus.st_size < 0 )
    {
      report.status       = RomFsReadStatus::StatFailed;
      report.error_number = EIO;
      static_cast<void>( ::close( file ) );
      return report;
    }

    report.file_size = static_cast<std::size_t>( fileStatus.st_size );
    if ( report.file_size > destination.size() )
    {
      report.status       = RomFsReadStatus::DestinationTooSmall;
      report.error_number = EOVERFLOW;
      static_cast<void>( ::close( file ) );
      return report;
    }

    while ( report.bytes_read < report.file_size )
    {
      const ssize_t readSize =
          ::read( file, destination.data() + report.bytes_read, report.file_size - report.bytes_read );
      if ( readSize < 0 )
      {
        if ( errno == EINTR )
        {
          continue;
        }

        report.status       = RomFsReadStatus::ReadFailed;
        report.error_number = errno;
        break;
      }

      if ( readSize == 0 )
      {
        report.status       = RomFsReadStatus::ReadFailed;
        report.error_number = EIO;
        break;
      }

      report.bytes_read += static_cast<std::size_t>( readSize );
    }

    if ( ::close( file ) != 0 && report.status == RomFsReadStatus::Success )
    {
      report.status       = RomFsReadStatus::CloseFailed;
      report.error_number = errno;
    }

    return report;
  }

  bool RomFs::isInitialized() const noexcept
  {
    return mIsInitialized;
  }
}  // namespace doggo::platform::nx
