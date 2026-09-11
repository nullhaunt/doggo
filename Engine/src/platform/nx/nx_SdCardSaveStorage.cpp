#include "doggo/platform/nx/nx_SdCardSaveStorage.hpp"

#include <switch.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
  constexpr char             DeviceName[]      = "sdmc";
  constexpr std::string_view StorageBackend    = "SD card";
  constexpr std::string_view StorageParentPath = ".";
  constexpr std::string_view StorageRootPath   = "doggo";
  constexpr std::string_view TemporarySuffix   = ".tmp";
  constexpr std::string_view BackupSuffix      = ".bak";

  using Report = doggo::save::SaveStorageReport;
  using Status = doggo::save::SaveStorageStatus;
  using Path   = std::array<char, FS_MAX_PATH>;

  [[nodiscard]] constexpr Result invariantFailure() noexcept
  {
    return MAKERESULT( Module_Libnx, LibnxError_BadInput );
  }

  void setPosixFailure( Report & report, const Status status, const int errorNumber ) noexcept
  {
    report.status        = status;
    report.native_result = fsdevGetLastResult();
    report.error_number  = errorNumber;
  }

  void setLocalFailure( Report & report, const Status status, const int errorNumber ) noexcept
  {
    report.status        = status;
    report.native_result = 0;
    report.error_number  = errorNumber;
  }

  [[nodiscard]] bool isValidName( const std::string_view name ) noexcept
  {
    if ( name.empty() || name == "." || name == ".." )
    {
      return false;
    }

    for ( const char character : name )
    {
      if ( character == '\0' || character == '/' || character == '\\' || character == ':' )
      {
        return false;
      }
    }

    return true;
  }

  [[nodiscard]] bool
  makePath( const std::string_view name, const std::string_view suffix, Path & destination, Report & report ) noexcept
  {
    if ( !isValidName( name ) )
    {
      setLocalFailure( report, Status::InvalidName, EINVAL );
      return false;
    }

    const std::size_t requiredSize = StorageRootPath.size() + 1 + name.size() + suffix.size() + 1;
    if ( requiredSize > destination.size() )
    {
      setLocalFailure( report, Status::InvalidName, ENAMETOOLONG );
      return false;
    }

    char * cursor = destination.data();
    std::memcpy( cursor, StorageRootPath.data(), StorageRootPath.size() );
    cursor += StorageRootPath.size();
    *cursor++ = '/';
    std::memcpy( cursor, name.data(), name.size() );
    cursor += name.size();
    std::memcpy( cursor, suffix.data(), suffix.size() );
    cursor += suffix.size();
    *cursor = '\0';
    return true;
  }

  [[nodiscard]] bool pathExists( const char * const path, bool & exists, Report & report ) noexcept
  {
    struct stat entryStatus = {};
    if ( stat( path, &entryStatus ) == 0 )
    {
      if ( !S_ISREG( entryStatus.st_mode ) )
      {
        setLocalFailure( report, Status::StatFailed, EISDIR );
        return false;
      }

      exists = true;
      return true;
    }

    const int errorNumber = errno;
    if ( errorNumber == ENOENT )
    {
      exists = false;
      return true;
    }

    setPosixFailure( report, Status::StatFailed, errorNumber );
    return false;
  }

  [[nodiscard]] bool isValidDirectory( const char * const path, bool & wasCreated, Report & report ) noexcept
  {
    struct stat entryStatus = {};
    if ( stat( path, &entryStatus ) == 0 )
    {
      if ( !S_ISDIR( entryStatus.st_mode ) )
      {
        setLocalFailure( report, Status::DirectoryFailed, ENOTDIR );
        return false;
      }

      return true;
    }

    const int statError = errno;
    if ( statError != ENOENT )
    {
      setPosixFailure( report, Status::DirectoryFailed, statError );
      return false;
    }

    if ( mkdir( path, 0777 ) != 0 )
    {
      setPosixFailure( report, Status::DirectoryFailed, errno );
      return false;
    }

    wasCreated = true;
    return true;
  }

  [[nodiscard]] bool commitDevice( Report & report ) noexcept
  {
    const Result result = fsdevCommitDevice( DeviceName );
    if ( R_FAILED( result ) )
    {
      report.status        = Status::CommitFailed;
      report.native_result = result;
      return false;
    }

    return true;
  }

  [[nodiscard]] bool removeIfPresent( const char * const path, bool & wasRemoved, Report & report ) noexcept
  {
    bool isPresent = false;
    if ( !pathExists( path, isPresent, report ) )
    {
      return false;
    }

    if ( !isPresent )
    {
      return true;
    }

    if ( unlink( path ) != 0 )
    {
      setPosixFailure( report, Status::RemoveFailed, errno );
      return false;
    }

    wasRemoved = true;
    return true;
  }

  [[nodiscard]] bool restoreBackupIfRequired( const char * const finalPath,
                                              const char * const backupPath,
                                              bool &             finalExists,
                                              bool &             backupExists,
                                              Report &           report ) noexcept
  {
    if ( finalExists || !backupExists )
    {
      return true;
    }

    if ( rename( backupPath, finalPath ) != 0 )
    {
      setPosixFailure( report, Status::RenameFailed, errno );
      return false;
    }

    if ( !commitDevice( report ) )
    {
      return false;
    }

    finalExists  = true;
    backupExists = false;
    return true;
  }
}  // namespace

namespace doggo::platform::nx
{
  SdCardSaveStorage::~SdCardSaveStorage()
  {
    static_cast<void>( finalize() );
  }

  std::uint32_t SdCardSaveStorage::initialize() noexcept
  {
    if ( mIsInitialized )
    {
      return 0;
    }

    if ( !fsdevGetDeviceFileSystem( DeviceName ) )
    {
      const Result result = fsdevMountSdmc();
      if ( R_FAILED( result ) )
      {
        return result;
      }

      mOwnsMount = true;
    }

    if ( !fsdevGetDeviceFileSystem( DeviceName ) )
    {
      if ( mOwnsMount )
      {
        static_cast<void>( fsdevUnmountDevice( DeviceName ) );
        mOwnsMount = false;
      }

      return invariantFailure();
    }

    mIsInitialized = true;
    return 0;
  }

  std::uint32_t SdCardSaveStorage::finalize() noexcept
  {
    if ( !mIsInitialized )
    {
      return 0;
    }

    if ( mOwnsMount && fsdevUnmountDevice( DeviceName ) != 0 )
    {
      return invariantFailure();
    }

    mOwnsMount     = false;
    mIsInitialized = false;
    return 0;
  }

  save::SaveStorageReport SdCardSaveStorage::readCommittedFile( const std::string_view name,
                                                                const std::span<std::uint8_t>
                                                                    destination ) noexcept
  {
    save::SaveStorageReport report;
    if ( !mIsInitialized )
    {
      return report;
    }

    Path finalPath  = {};
    Path backupPath = {};
    if ( !makePath( name, {}, finalPath, report ) || !makePath( name, BackupSuffix, backupPath, report ) )
    {
      return report;
    }

    bool finalExists  = false;
    bool backupExists = false;
    if ( !pathExists( finalPath.data(), finalExists, report ) ||
         !pathExists( backupPath.data(), backupExists, report ) ||
         !restoreBackupIfRequired( finalPath.data(), backupPath.data(), finalExists, backupExists, report ) )
    {
      return report;
    }

    if ( !finalExists )
    {
      report.status        = save::SaveStorageStatus::NotFound;
      report.native_result = fsdevGetLastResult();
      report.error_number  = ENOENT;
      return report;
    }

    const int file = ::open( finalPath.data(), O_RDONLY );
    if ( file < 0 )
    {
      setPosixFailure( report, save::SaveStorageStatus::OpenFailed, errno );
      return report;
    }

    struct stat fileStatus = {};
    if ( fstat( file, &fileStatus ) != 0 )
    {
      setPosixFailure( report, save::SaveStorageStatus::StatFailed, errno );
      static_cast<void>( ::close( file ) );
      return report;
    }

    if ( fileStatus.st_size < 0 )
    {
      report.status       = save::SaveStorageStatus::StatFailed;
      report.error_number = EIO;
      static_cast<void>( ::close( file ) );
      return report;
    }

    report.file_size = static_cast<std::size_t>( fileStatus.st_size );
    if ( report.file_size > destination.size() )
    {
      report.status       = save::SaveStorageStatus::DestinationTooSmall;
      report.error_number = EOVERFLOW;
      static_cast<void>( ::close( file ) );
      return report;
    }

    report.status = save::SaveStorageStatus::Success;
    while ( report.bytes_transferred < report.file_size )
    {
      const ssize_t readSize =
          read( file, destination.data() + report.bytes_transferred, report.file_size - report.bytes_transferred );
      if ( readSize < 0 )
      {
        if ( errno == EINTR )
        {
          continue;
        }

        setPosixFailure( report, save::SaveStorageStatus::ReadFailed, errno );
        break;
      }

      if ( readSize == 0 )
      {
        report.status       = save::SaveStorageStatus::ReadFailed;
        report.error_number = EIO;
        break;
      }

      report.bytes_transferred += static_cast<std::size_t>( readSize );
    }

    if ( close( file ) != 0 && report.status == save::SaveStorageStatus::Success )
    {
      setPosixFailure( report, save::SaveStorageStatus::CloseFailed, errno );
    }

    return report;
  }

  save::SaveStorageReport SdCardSaveStorage::writeCommittedFile( const std::string_view name,
                                                                 const std::span<const std::uint8_t>
                                                                     source ) noexcept
  {
    save::SaveStorageReport report;
    report.file_size = source.size();
    if ( !mIsInitialized )
    {
      return report;
    }

    Path finalPath     = {};
    Path temporaryPath = {};
    Path backupPath    = {};
    if ( !makePath( name, {}, finalPath, report ) ||
         !makePath( name, TemporarySuffix, temporaryPath, report ) ||
         !makePath( name, BackupSuffix, backupPath, report ) )
    {
      return report;
    }

    bool isDirectoryCreated = false;
    if ( !isValidDirectory( StorageParentPath.data(), isDirectoryCreated, report ) ||
         !isValidDirectory( StorageRootPath.data(), isDirectoryCreated, report ) )
    {
      return report;
    }

    if ( isDirectoryCreated && !commitDevice( report ) )
    {
      return report;
    }

    bool finalExists     = false;
    bool temporaryExists = false;
    bool backupExists    = false;
    if ( !pathExists( finalPath.data(), finalExists, report ) ||
         !pathExists( temporaryPath.data(), temporaryExists, report ) ||
         !pathExists( backupPath.data(), backupExists, report ) ||
         !restoreBackupIfRequired( finalPath.data(), backupPath.data(), finalExists, backupExists, report ) )
    {
      return report;
    }

    bool cleanupChanged = false;
    if ( !removeIfPresent( temporaryPath.data(), cleanupChanged, report ) )
    {
      return report;
    }

    if ( finalExists && !removeIfPresent( backupPath.data(), cleanupChanged, report ) )
    {
      return report;
    }

    if ( cleanupChanged && !commitDevice( report ) )
    {
      return report;
    }

    const int file = open( temporaryPath.data(), O_WRONLY | O_CREAT | O_TRUNC, 0666 );
    if ( file < 0 )
    {
      setPosixFailure( report, save::SaveStorageStatus::OpenFailed, errno );
      return report;
    }

    report.status = save::SaveStorageStatus::Success;
    while ( report.bytes_transferred < source.size() )
    {
      const ssize_t writeSize =
          write( file, source.data() + report.bytes_transferred, source.size() - report.bytes_transferred );
      if ( writeSize < 0 )
      {
        if ( errno == EINTR )
        {
          continue;
        }

        setPosixFailure( report, save::SaveStorageStatus::WriteFailed, errno );
        break;
      }

      if ( writeSize == 0 )
      {
        report.status       = save::SaveStorageStatus::WriteFailed;
        report.error_number = EIO;
        break;
      }

      report.bytes_transferred += static_cast<std::size_t>( writeSize );
    }

    if ( report.status == save::SaveStorageStatus::Success && fsync( file ) != 0 )
    {
      setPosixFailure( report, save::SaveStorageStatus::FlushFailed, errno );
    }

    if ( close( file ) != 0 && report.status == save::SaveStorageStatus::Success )
    {
      setPosixFailure( report, save::SaveStorageStatus::CloseFailed, errno );
    }

    if ( report.status != save::SaveStorageStatus::Success || !commitDevice( report ) )
    {
      return report;
    }

    bool wasRotated = false;
    if ( finalExists )
    {
      if ( rename( finalPath.data(), backupPath.data() ) != 0 )
      {
        setPosixFailure( report, save::SaveStorageStatus::RenameFailed, errno );
        return report;
      }

      wasRotated = true;
      if ( !commitDevice( report ) )
      {
        return report;
      }
    }

    if ( rename( temporaryPath.data(), finalPath.data() ) != 0 )
    {
      setPosixFailure( report, save::SaveStorageStatus::RenameFailed, errno );
      const save::SaveStorageReport promotionFailure = report;
      if ( wasRotated && rename( backupPath.data(), finalPath.data() ) == 0 )
      {
        static_cast<void>( fsdevCommitDevice( DeviceName ) );
      }

      return promotionFailure;
    }

    if ( !commitDevice( report ) )
    {
      return report;
    }

    if ( wasRotated )
    {
      bool backupRemoved = false;
      if ( !removeIfPresent( backupPath.data(), backupRemoved, report ) ||
           ( backupRemoved && !commitDevice( report ) ) )
      {
        return report;
      }
    }

    report.status = save::SaveStorageStatus::Success;
    return report;
  }

  std::string_view SdCardSaveStorage::backendName() const noexcept
  {
    return StorageBackend;
  }

  std::string_view SdCardSaveStorage::rootPath() const noexcept
  {
    return StorageRootPath;
  }

  bool SdCardSaveStorage::isInitialized() const noexcept
  {
    return mIsInitialized;
  }
}  // namespace doggo::platform::nx
