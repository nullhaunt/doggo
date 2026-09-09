#include "Arguments.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
  [[nodiscard]] const char * readEnvironment( const char * const name ) noexcept
  {
    const char * const value = std::getenv( name );
    return value && value[ 0 ] != '\0' ? value : nullptr;
  }

  [[nodiscard]] bool readValue( const int argc, char * const argv[], int & index, const std::string_view option,
                                std::string & value )
  {
    if ( index + 1 >= argc )
    {
      std::cerr << "Missing value for " << option << ".\n";
      return false;
    }

    value = argv[ ++index ];
    return true;
  }

}  // namespace

namespace doggo::devtool
{
  void printUsage()
  {
    std::cout << "DOGGO deploy-and-log tool\n\n"
                 "Usage:\n"
                 "  doggo-dev [options] <file.nro> [-- <application arguments>]\n\n"
                 "Options:\n"
                 "  --switch <host>       Switch address (or DOGGO_SWITCH_IP)\n"
                 "  --remote-path <path>  Upload name/path (default: NRO filename)\n"
                 "  --help, -h            Show this help\n\n"
                 "DoggoDev never configures or builds the game. Build the NRO in your IDE,\n"
                 "then pass that artifact to this process.\n";
  }

  std::optional<Options> parseOptions( const int argc, char * const argv[] )
  {
    if ( argc == 1 )
    {
      printUsage();
      return std::nullopt;
    }

    Options options;
    if ( const char * const configured = readEnvironment( "DOGGO_SWITCH_IP" ) )
    {
      options.switch_address = configured;
    }

    for ( int index = 1; index < argc; ++index )
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

      if ( argument == "build" || argument == "run" )
      {
        std::cerr << "The '" << argument
                  << "' command was removed. DoggoDev only deploys an existing NRO; pass its path directly.\n";
        return std::nullopt;
      }

      if ( argument == "--switch" )
      {
        if ( !readValue( argc, argv, index, argument, options.switch_address ) )
        {
          return std::nullopt;
        }
      }
      else if ( argument == "--remote-path" )
      {
        if ( !readValue( argc, argv, index, argument, options.remote_path ) )
        {
          return std::nullopt;
        }
      }
      else if ( !argument.empty() && argument.front() == '-' )
      {
        std::cerr << "Unknown option: " << argument << "\n";
        return std::nullopt;
      }
      else if ( options.nro.empty() )
      {
        options.nro = std::filesystem::path{ argument };
      }
      else
      {
        std::cerr << "Unexpected argument: " << argument << "\n"
                  << "Put application arguments after --.\n";
        return std::nullopt;
      }
    }

    if ( options.nro.empty() )
    {
      std::cerr << "An NRO path is required.\n";
      return std::nullopt;
    }
    if ( options.switch_address.empty() )
    {
      std::cerr << "Switch address is required; use --switch <host> or DOGGO_SWITCH_IP.\n";
      return std::nullopt;
    }
    if ( options.remote_path.empty() )
    {
      options.remote_path = options.nro.filename().generic_string();
    }

    return options;
  }
}  // namespace doggo::devtool
