#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
  #include <process.h>
#else
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <unistd.h>
#endif

namespace
{
  namespace fs = std::filesystem;

  enum class Command
  {
    Build,
    Run,
  };

  struct Options final
  {
      Command                    command = Command::Run;
      std::string                configuration{ "debug" };
      std::optional<fs::path>    repository;
      std::optional<fs::path>    nro;
      std::optional<std::string> switch_address;
      std::optional<std::string> nxlink;
      std::string                cmake{ "cmake" };
      bool                       should_build = true;
      std::vector<std::string>   application_arguments;
  };

  void printUsage()
  {
    std::cout << "DOGGO deploy-and-log tool\n\n"
                 "Usage:\n"
                 "  doggo-dev build [options]\n"
                 "  doggo-dev run [options] [-- <application arguments>]\n\n"
                 "Options:\n"
                 "  --switch <host>       Switch address (or DOGGO_SWITCH_IP)\n"
                 "  --config <name>       debug or release (default: debug)\n"
                 "  --repo <path>         DOGGO repository root\n"
                 "  --nro <path>          NRO to upload\n"
                 "  --nxlink <path>       nxlink executable (or DOGGO_NXLINK)\n"
                 "  --cmake <path>        CMake executable (default: cmake)\n"
                 "  --no-build            Skip configure and build before run\n"
                 "  --help, -h            Show this help\n";
  }

  [[nodiscard]] const char * readEnvironment( const char * const name ) noexcept
  {
    const char * const value = std::getenv( name );
    return value && value[ 0 ] != '\0' ? value : nullptr;
  }

  [[nodiscard]] std::optional<fs::path> findRepositoryRoot( fs::path current )
  {
    current = fs::absolute( current );
    while ( true )
    {
      if ( fs::is_regular_file( current / "CMakePresets.json" ) &&
           fs::is_directory( current / "Engine" ) &&
           fs::is_directory( current / "Game" ) )
      {
        return current;
      }

      const fs::path parent = current.parent_path();
      if ( parent == current )
      {
        return std::nullopt;
      }

      current = parent;
    }
  }

  [[nodiscard]] bool readOptionValue( const int              argc,
                                      char * const           argv[],
                                      int &                  index,
                                      const std::string_view option,
                                      std::string &          value )
  {
    if ( index + 1 >= argc )
    {
      std::cerr << "Missing value for " << option << ".\n";
      return false;
    }

    value = argv[ ++index ];
    return true;
  }

  [[nodiscard]] std::optional<Options> parseOptions( const int argc, char * const argv[] )
  {
    if ( argc < 2 )
    {
      printUsage();
      return std::nullopt;
    }

    Options                options;
    const std::string_view command = argv[ 1 ];
    if ( command == "build" )
    {
      options.command = Command::Build;
    }
    else if ( command == "run" )
    {
      options.command = Command::Run;
    }
    else if ( command == "--help" || command == "-h" )
    {
      printUsage();
      std::exit( EXIT_SUCCESS );
    }
    else
    {
      std::cerr << "Unknown command: " << command << "\n";
      return std::nullopt;
    }

    for ( int index = 2; index < argc; ++index )
    {
      const std::string_view argument = argv[ index ];
      if ( argument == "--" )
      {
        for ( ++index; index < argc; ++index )
        {
          options.application_arguments.emplace_back( argv[ index ] );
        }
        break;
      }

      if ( argument == "--help" || argument == "-h" )
      {
        printUsage();
        std::exit( EXIT_SUCCESS );
      }
      if ( argument == "--no-build" )
      {
        options.should_build = false;
        continue;
      }

      std::string value;
      if ( argument == "--switch" )
      {
        if ( !readOptionValue( argc, argv, index, argument, value ) )
        {
          return std::nullopt;
        }
        options.switch_address = std::move( value );
      }
      else if ( argument == "--config" )
      {
        if ( !readOptionValue( argc, argv, index, argument, options.configuration ) )
        {
          return std::nullopt;
        }
      }
      else if ( argument == "--repo" )
      {
        if ( !readOptionValue( argc, argv, index, argument, value ) )
        {
          return std::nullopt;
        }
        options.repository = fs::path{ value };
      }
      else if ( argument == "--nro" )
      {
        if ( !readOptionValue( argc, argv, index, argument, value ) )
        {
          return std::nullopt;
        }
        options.nro = fs::path{ value };
      }
      else if ( argument == "--nxlink" )
      {
        if ( !readOptionValue( argc, argv, index, argument, value ) )
        {
          return std::nullopt;
        }
        options.nxlink = std::move( value );
      }
      else if ( argument == "--cmake" )
      {
        if ( !readOptionValue( argc, argv, index, argument, options.cmake ) )
        {
          return std::nullopt;
        }
      }
      else
      {
        std::cerr << "Unknown option: " << argument << "\n";
        return std::nullopt;
      }
    }

    if ( options.configuration != "debug" && options.configuration != "release" )
    {
      std::cerr << "--config must be debug or release.\n";
      return std::nullopt;
    }

    return options;
  }

  void printCommand( const std::vector<std::string> & arguments )
  {
    std::cout << '$';
    for ( const std::string & argument : arguments )
    {
      std::cout << ' ';
      if ( argument.find_first_of( " \t\"" ) == std::string::npos )
      {
        std::cout << argument;
      }
      else
      {
        std::cout << '"';
        for ( const char character : argument )
        {
          if ( character == '"' || character == '\\' )
          {
            std::cout << '\\';
          }
          std::cout << character;
        }
        std::cout << '"';
      }
    }
    std::cout << '\n' << std::flush;
  }

  [[nodiscard]] int runProcess( const std::vector<std::string> & arguments )
  {
    printCommand( arguments );

#ifdef _WIN32
    std::vector<const char *> nativeArguments;
#else
    std::vector<char *> nativeArguments;
#endif
    nativeArguments.reserve( arguments.size() + 1 );
    for ( const std::string & argument : arguments )
    {
#ifdef _WIN32
      nativeArguments.push_back( argument.c_str() );
#else
      nativeArguments.push_back( const_cast<char *>( argument.c_str() ) );
#endif
    }
    nativeArguments.push_back( nullptr );

#ifdef _WIN32
    const std::intptr_t result = _spawnvp( _P_WAIT, nativeArguments.front(), nativeArguments.data() );
    if ( result == -1 )
    {
      std::cerr << "Unable to start " << arguments.front() << ": " << std::strerror( errno ) << '\n';
      return EXIT_FAILURE;
    }

    return static_cast<int>( result );
#else
    const pid_t child = fork();
    if ( child == -1 )
    {
      std::cerr << "Unable to start " << arguments.front() << ": " << std::strerror( errno ) << '\n';
      return EXIT_FAILURE;
    }

    if ( child == 0 )
    {
      execvp( nativeArguments.front(), nativeArguments.data() );
      std::cerr << "Unable to start " << arguments.front() << ": " << std::strerror( errno ) << '\n';
      _exit( 127 );
    }

    int status = 0;
    while ( waitpid( child, &status, 0 ) == -1 )
    {
      if ( errno != EINTR )
      {
        std::cerr << "Unable to wait for " << arguments.front() << ": " << std::strerror( errno ) << '\n';
        return EXIT_FAILURE;
      }
    }

    if ( WIFEXITED( status ) )
    {
      return WEXITSTATUS( status );
    }

    if ( WIFSIGNALED( status ) )
    {
      return 128 + WTERMSIG( status );
    }

    return EXIT_FAILURE;
#endif
  }

  [[nodiscard]] std::string findNxlink( const Options & options )
  {
    if ( options.nxlink )
    {
      return *options.nxlink;
    }

    if ( const char * const configured = readEnvironment( "DOGGO_NXLINK" ) )
    {
      return configured;
    }

    if ( const char * const devkitPro = readEnvironment( "DEVKITPRO" ) )
    {
#ifdef _WIN32
      const fs::path candidate = fs::path{ devkitPro } / "tools" / "bin" / "nxlink.exe";
#else
      const fs::path candidate = fs::path{ devkitPro } / "tools" / "bin" / "nxlink";
#endif
      if ( fs::is_regular_file( candidate ) )
      {
        return candidate.string();
      }
    }
    return "nxlink";
  }
}  // namespace

int main( const int argc, char * const argv[] )
{
  const std::optional<Options> parsedOptions = parseOptions( argc, argv );
  if ( !parsedOptions )
  {
    return EXIT_FAILURE;
  }
  const Options & options = *parsedOptions;

  try
  {
    const std::optional<fs::path> repository = options.repository ? std::optional{ fs::absolute( *options.repository ) }
                                                                  : findRepositoryRoot( fs::current_path() );
    if ( !repository )
    {
      std::cerr << "DOGGO repository root was not found; use --repo <path>.\n";
      return EXIT_FAILURE;
    }

    fs::current_path( *repository );

    const std::string configurePreset = "doggo-" + options.configuration;
    const std::string buildPreset     = "game-" + options.configuration;

    if ( options.command == Command::Build || options.should_build )
    {
      int result = runProcess( { options.cmake, "--preset", configurePreset } );
      if ( result != EXIT_SUCCESS )
      {
        return result;
      }

      result = runProcess( { options.cmake, "--build", "--preset", buildPreset } );
      if ( result != EXIT_SUCCESS )
      {
        return result;
      }
    }

    if ( options.command == Command::Build )
    {
      return EXIT_SUCCESS;
    }

    std::string switchAddress;
    if ( options.switch_address )
    {
      switchAddress = *options.switch_address;
    }
    else if ( const char * const configured = readEnvironment( "DOGGO_SWITCH_IP" ) )
    {
      switchAddress = configured;
    }
    else
    {
      std::cerr << "Switch address is required; use --switch <host> or DOGGO_SWITCH_IP.\n";
      return EXIT_FAILURE;
    }

    const fs::path nro = options.nro ? fs::absolute( *options.nro )
                                     : *repository / "build" / configurePreset / "Game" / "doggo_game.nro";
    if ( !fs::is_regular_file( nro ) )
    {
      std::cerr << "NRO not found: " << nro << '\n';
      return EXIT_FAILURE;
    }

    std::vector<std::string> nxlinkArguments{ findNxlink( options ), "-s", "-a", switchAddress, "--", nro.string() };
    nxlinkArguments.insert( nxlinkArguments.end(),
                            options.application_arguments.begin(),
                            options.application_arguments.end() );
    return runProcess( nxlinkArguments );
  }
  catch ( const fs::filesystem_error & error )
  {
    std::cerr << "Filesystem error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
