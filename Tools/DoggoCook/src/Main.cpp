#include "ShaderCooker.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  void printUsage()
  {
    std::cout << "DOGGO asset cooker\n\n"
                 "Usage:\n"
                 "  doggo-cook shader --manifest <file> --abi <file> --output <directory> --uam <executable>\n"
                 "  doggo-cook shader-abi --abi <file> --output <header>\n\n"
                 "Commands:\n"
                 "  shader      Validate and compile a shader program and its variants to DKSH\n"
                 "  shader-abi  Validate the binding ABI and generate its C++ header\n";
  }

  [[nodiscard]] std::optional<std::filesystem::path> readPathOption( const int argc, char * const argv[], int & index,
                                                                     const std::string_view option )
  {
    if ( index + 1 >= argc )
    {
      throw std::runtime_error{ "Missing value for " + std::string{ option } + '.' };
    }

    return std::filesystem::path{ argv[ ++index ] };
  }

  [[nodiscard]] int runShaderCommand( const int argc, char * const argv[] )
  {
    doggo::cook::ShaderCookOptions options;
    std::filesystem::path          uam;

    for ( int index = 2; index < argc; ++index )
    {
      const std::string_view argument = argv[ index ];
      if ( argument == "--manifest" )
      {
        options.manifest = *readPathOption( argc, argv, index, argument );
      }
      else if ( argument == "--abi" )
      {
        options.abi = *readPathOption( argc, argv, index, argument );
      }
      else if ( argument == "--output" )
      {
        options.output_directory = *readPathOption( argc, argv, index, argument );
      }
      else if ( argument == "--uam" )
      {
        uam = *readPathOption( argc, argv, index, argument );
      }
      else
      {
        throw std::runtime_error{ "Unknown shader option: " + std::string{ argument } };
      }
    }

    if ( options.manifest.empty() || options.abi.empty() || options.output_directory.empty() || uam.empty() )
    {
      throw std::runtime_error{ "shader requires --manifest, --abi, --output, and --uam" };
    }

    doggo::cook::UamCompiler compiler{ std::move( uam ) };
    doggo::cook::cookShaderProgram( options, compiler );
    return EXIT_SUCCESS;
  }

  [[nodiscard]] int runShaderAbiCommand( const int argc, char * const argv[] )
  {
    std::filesystem::path abi;
    std::filesystem::path output;

    for ( int index = 2; index < argc; ++index )
    {
      const std::string_view argument = argv[ index ];
      if ( argument == "--abi" )
      {
        abi = *readPathOption( argc, argv, index, argument );
      }
      else if ( argument == "--output" )
      {
        output = *readPathOption( argc, argv, index, argument );
      }
      else
      {
        throw std::runtime_error{ "Unknown shader-abi option: " + std::string{ argument } };
      }
    }

    if ( abi.empty() || output.empty() )
    {
      throw std::runtime_error{ "shader-abi requires --abi and --output" };
    }

    doggo::cook::generateShaderBindingHeader( abi, output );
    return EXIT_SUCCESS;
  }
}  // namespace

int main( const int argc, char * const argv[] )
{
  try
  {
    if ( argc < 2 )
    {
      printUsage();
      return EXIT_FAILURE;
    }

    const std::string_view command = argv[ 1 ];
    if ( command == "--help" || command == "-h" )
    {
      printUsage();
      return EXIT_SUCCESS;
    }
    if ( command == "shader" )
    {
      return runShaderCommand( argc, argv );
    }
    if ( command == "shader-abi" )
    {
      return runShaderAbiCommand( argc, argv );
    }

    throw std::runtime_error{ "Unknown command: " + std::string{ command } };
  }
  catch ( const std::exception & error )
  {
    std::cerr << "doggo-cook: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
