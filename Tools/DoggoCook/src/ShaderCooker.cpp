#include "ShaderCooker.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
  enum class BindingKind : std::uint8_t
  {
    UniformBuffer,
    StorageBuffer,
    Sampler,
    Image,
  };

  struct Binding final
  {
      BindingKind   kind;
      std::uint32_t slot;
      std::string   name;
      std::string   macro;
  };

  struct ShaderAbi final
  {
      std::uint32_t        version = 0;
      std::vector<Binding> bindings;
  };

  struct ShaderDefine final
  {
      std::string name;
      std::string value;
  };

  struct ShaderVariant final
  {
      std::string               name;
      std::vector<ShaderDefine> defines;
  };

  struct ShaderSource final
  {
      doggo::cook::ShaderStage stage;
      std::filesystem::path    path;
  };

  struct ShaderProgramManifest final
  {
      std::uint32_t              version = 0;
      std::string                program;
      std::vector<ShaderSource>  sources;
      std::vector<ShaderVariant> variants;
  };

  struct StageInterface final
  {
      std::map<std::uint32_t, std::string> inputs;
      std::map<std::uint32_t, std::string> outputs;
  };

  struct PendingShader final
  {
      std::string              variant;
      doggo::cook::ShaderStage stage;
      std::filesystem::path    temporary;
      std::filesystem::path    destination;
      std::uintmax_t           size = 0;
      std::uint64_t            hash = 0;
  };

  constexpr std::uint64_t FnvOffsetBasis = 0xCBF29CE484222325ULL;
  constexpr std::uint64_t FnvPrime       = 0x100000001B3ULL;

  [[nodiscard]] std::string trim( std::string value )
  {
    const auto isNotSpace = []( const unsigned char character )
    {
      return std::isspace( character ) == 0;
    };

    const auto begin = std::ranges::find_if( value, isNotSpace );
    const auto end   = std::find_if( value.rbegin(), value.rend(), isNotSpace ).base();

    if ( begin >= end )
    {
      return {};
    }

    return std::string{ begin, end };
  }

  [[nodiscard]] std::string collapseWhitespace( const std::string_view value )
  {
    std::string result;
    bool        pendingSpace = false;

    for ( const unsigned char character : value )
    {
      if ( std::isspace( character ) != 0 )
      {
        pendingSpace = !result.empty();
      }
      else
      {
        if ( pendingSpace )
        {
          result.push_back( ' ' );
          pendingSpace = false;
        }
        result.push_back( static_cast<char>( character ) );
      }
    }

    return result;
  }

  [[nodiscard]] std::vector<std::uint8_t> readBytes( const std::filesystem::path & path )
  {
    std::ifstream stream{ path, std::ios::binary };
    if ( !stream )
    {
      throw std::runtime_error{ "Unable to open " + path.string() };
    }

    stream.seekg( 0, std::ios::end );
    const std::streamoff end = stream.tellg();

    if ( end < 0 )
    {
      throw std::runtime_error{ "Unable to determine the size of " + path.string() };
    }
    stream.seekg( 0, std::ios::beg );

    std::vector<std::uint8_t> bytes( static_cast<std::size_t>( end ) );
    if ( !bytes.empty() )
    {
      stream.read( reinterpret_cast<char *>( bytes.data() ), static_cast<std::streamsize>( bytes.size() ) );
      if ( !stream )
      {
        throw std::runtime_error{ "Unable to read " + path.string() };
      }
    }

    return bytes;
  }

  [[nodiscard]] std::string readText( const std::filesystem::path & path )
  {
    const std::vector<std::uint8_t> bytes = readBytes( path );
    if ( bytes.empty() )
    {
      return {};
    }

    return std::string{ reinterpret_cast<const char *>( bytes.data() ), bytes.size() };
  }

  void writeBytes( const std::filesystem::path & path, const std::vector<std::uint8_t> & bytes )
  {
    std::filesystem::create_directories( path.parent_path() );
    std::ofstream stream{ path, std::ios::binary | std::ios::trunc };
    if ( !stream )
    {
      throw std::runtime_error{ "Unable to create " + path.string() };
    }

    if ( !bytes.empty() )
    {
      stream.write( reinterpret_cast<const char *>( bytes.data() ), static_cast<std::streamsize>( bytes.size() ) );
    }

    if ( !stream )
    {
      throw std::runtime_error{ "Unable to write " + path.string() };
    }
  }

  void writeTextIfDifferent( const std::filesystem::path & path, const std::string_view text )
  {
    const std::vector<std::uint8_t> desired{ text.begin(), text.end() };
    if ( std::filesystem::is_regular_file( path ) && readBytes( path ) == desired )
    {
      return;
    }

    const std::filesystem::path temporary = path.string() + ".tmp";
    writeBytes( temporary, desired );
    std::error_code ignored;
    std::filesystem::remove( path, ignored );
    std::filesystem::rename( temporary, path );
  }

  void installIfDifferent( const std::filesystem::path & temporary, const std::filesystem::path & destination )
  {
    if ( std::filesystem::is_regular_file( destination ) && readBytes( temporary ) == readBytes( destination ) )
    {
      std::filesystem::remove( temporary );
      return;
    }

    std::filesystem::create_directories( destination.parent_path() );
    std::error_code ignored;
    std::filesystem::remove( destination, ignored );
    std::filesystem::rename( temporary, destination );
  }

  [[nodiscard]] std::uint64_t fnv1a64( const std::vector<std::uint8_t> & bytes ) noexcept
  {
    std::uint64_t hash = FnvOffsetBasis;
    for ( const std::uint8_t byte : bytes )
    {
      hash ^= byte;
      hash *= FnvPrime;
    }

    return hash;
  }

  [[nodiscard]] std::uint32_t parseUnsigned( const std::string_view token, const std::string_view context )
  {
    std::uint32_t value       = 0;
    const auto [ end, error ] = std::from_chars( token.data(), token.data() + token.size(), value );
    if ( error != std::errc{} || end != token.data() + token.size() )
    {
      throw std::runtime_error{
          "Invalid unsigned integer '" + std::string{ token } + "' in " + std::string{ context } };
    }

    return value;
  }

  [[nodiscard]] bool isIdentifier( const std::string_view value )
  {
    if ( value.empty() || ( std::isalpha( static_cast<unsigned char>( value.front() ) ) == 0 && value.front() != '_' ) )
    {
      return false;
    }

    return std::ranges::all_of( value.substr( 1 ),
                                []( const unsigned char character )
                                {
                                  return std::isalnum( character ) != 0 || character == '_';
                                } );
  }

  [[nodiscard]] bool isLowerSnakeIdentifier( const std::string_view value )
  {
    if ( value.empty() || std::islower( static_cast<unsigned char>( value.front() ) ) == 0 )
    {
      return false;
    }

    return std::ranges::all_of( value.substr( 1 ),
                                []( const unsigned char character )
                                {
                                  return std::islower( character ) != 0 ||
                                         std::isdigit( character ) != 0 ||
                                         character == '_';
                                } );
  }

  [[nodiscard]] std::string uppercase( std::string value )
  {
    std::ranges::transform( value, value.begin(),
                            []( const unsigned char character )
                            {
                              return static_cast<char>( std::toupper( character ) );
                            } );
    return value;
  }

  [[nodiscard]] std::string pascalCase( const std::string_view value )
  {
    std::string result;
    bool        isCapitalized = true;

    for ( const unsigned char character : value )
    {
      if ( character == '_' )
      {
        isCapitalized = true;
      }
      else
      {
        result.push_back( isCapitalized ? static_cast<char>( std::toupper( character ) )
                                        : static_cast<char>( character ) );
        isCapitalized = false;
      }
    }

    return result;
  }

  [[nodiscard]] std::vector<std::string> tokenize( const std::string_view line, const std::size_t lineNumber,
                                                   const std::filesystem::path & path )
  {
    std::vector<std::string> tokens;
    std::size_t              position = 0;

    while ( position < line.size() )
    {
      while ( position < line.size() && std::isspace( static_cast<unsigned char>( line[ position ] ) ) != 0 )
      {
        ++position;
      }

      if ( position == line.size() || line[ position ] == '#' )
      {
        break;
      }

      std::string token;
      if ( line[ position ] == '"' )
      {
        ++position;
        bool isClosed = false;

        while ( position < line.size() )
        {
          const char character = line[ position++ ];
          if ( character == '"' )
          {
            isClosed = true;
            break;
          }

          if ( character == '\\' && position < line.size() )
          {
            token.push_back( line[ position++ ] );
          }
          else
          {
            token.push_back( character );
          }
        }

        if ( !isClosed )
        {
          throw std::runtime_error{
              path.string() + ':' + std::to_string( lineNumber ) + ": unterminated quoted token" };
        }
      }
      else
      {
        const std::size_t begin = position;
        while ( position < line.size() && std::isspace( static_cast<unsigned char>( line[ position ] ) ) == 0 )
        {
          ++position;
        }

        token.assign( line.substr( begin, position - begin ) );
      }

      tokens.push_back( std::move( token ) );
    }

    return tokens;
  }

  [[nodiscard]] std::vector<std::vector<std::string>> readTokenLines( const std::filesystem::path & path )
  {
    std::ifstream stream{ path };
    if ( !stream )
    {
      throw std::runtime_error{ "Unable to open " + path.string() };
    }

    std::vector<std::vector<std::string>> lines;
    std::string                           line;
    std::size_t                           lineNumber = 0;

    while ( std::getline( stream, line ) )
    {
      ++lineNumber;
      std::vector<std::string> tokens = tokenize( line, lineNumber, path );
      if ( !tokens.empty() )
      {
        lines.push_back( std::move( tokens ) );
      }
    }

    if ( !stream.eof() )
    {
      throw std::runtime_error{ "Unable to read " + path.string() };
    }

    return lines;
  }

  [[nodiscard]] BindingKind parseBindingKind( const std::string_view value )
  {
    if ( value == "ubo" )
    {
      return BindingKind::UniformBuffer;
    }

    if ( value == "ssbo" )
    {
      return BindingKind::StorageBuffer;
    }

    if ( value == "sampler" )
    {
      return BindingKind::Sampler;
    }

    if ( value == "image" )
    {
      return BindingKind::Image;
    }

    throw std::runtime_error{ "Unknown shader binding kind: " + std::string{ value } };
  }

  [[nodiscard]] std::string_view bindingKindToken( const BindingKind kind ) noexcept
  {
    switch ( kind )
    {
      case BindingKind::UniformBuffer:
        return "ubo";
      case BindingKind::StorageBuffer:
        return "ssbo";
      case BindingKind::Sampler:
        return "sampler";
      case BindingKind::Image:
        return "image";
    }

    return "unknown";
  }

  [[nodiscard]] std::uint32_t bindingLimit( const BindingKind kind ) noexcept
  {
    switch ( kind )
    {
      case BindingKind::UniformBuffer:
      case BindingKind::StorageBuffer:
        return 16;
      case BindingKind::Sampler:
        return 32;
      case BindingKind::Image:
        return 8;
    }

    return 0;
  }

  [[nodiscard]] std::string bindingMacro( const BindingKind kind, const std::string_view name )
  {
    return "DOGGO_" + uppercase( std::string{ bindingKindToken( kind ) } ) + '_' + uppercase( std::string{ name } );
  }

  [[nodiscard]] ShaderAbi parseShaderAbi( const std::filesystem::path & path )
  {
    const std::vector<std::vector<std::string>> lines = readTokenLines( path );
    if ( lines.empty() || lines.front().size() != 2 || lines.front()[ 0 ] != "doggo_shader_abi" )
    {
      throw std::runtime_error{ path.string() + ": expected 'doggo_shader_abi <version>'" };
    }

    ShaderAbi abi;
    abi.version = parseUnsigned( lines.front()[ 1 ], path.string() );
    if ( abi.version == 0 )
    {
      throw std::runtime_error{ path.string() + ": ABI version must be non-zero" };
    }

    std::set<std::pair<BindingKind, std::uint32_t>> usedSlots;
    std::set<std::string>                           usedMacros;
    for ( std::size_t index = 1; index < lines.size(); ++index )
    {
      const std::vector<std::string> & tokens = lines[ index ];
      if ( tokens.size() != 3 )
      {
        throw std::runtime_error{ path.string() + ": binding entries require '<kind> <slot> <name>'" };
      }

      Binding binding{
          .kind  = parseBindingKind( tokens[ 0 ] ),
          .slot  = parseUnsigned( tokens[ 1 ], path.string() ),
          .name  = tokens[ 2 ],
          .macro = {},
      };

      if ( binding.slot >= bindingLimit( binding.kind ) )
      {
        throw std::runtime_error{ path.string() +
                                  ": " +
                                  std::string{ bindingKindToken( binding.kind ) } +
                                  " binding " +
                                  std::to_string( binding.slot ) +
                                  " exceeds the UAM limit" };
      }

      if ( !isLowerSnakeIdentifier( binding.name ) )
      {
        throw std::runtime_error{ path.string() + ": binding names must be lower_snake_case: " + binding.name };
      }

      binding.macro = bindingMacro( binding.kind, binding.name );
      if ( !usedSlots.emplace( binding.kind, binding.slot ).second )
      {
        throw std::runtime_error{ path.string() +
                                  ": duplicate " +
                                  std::string{ bindingKindToken( binding.kind ) } +
                                  " binding " +
                                  std::to_string( binding.slot ) };
      }

      if ( !usedMacros.insert( binding.macro ).second )
      {
        throw std::runtime_error{ path.string() + ": duplicate binding name: " + binding.name };
      }

      abi.bindings.push_back( std::move( binding ) );
    }

    std::ranges::sort( abi.bindings,
                       []( const Binding & left, const Binding & right )
                       {
                         return std::pair{ left.kind, left.slot } < std::pair{ right.kind, right.slot };
                       } );
    return abi;
  }

  [[nodiscard]] doggo::cook::ShaderStage parseShaderStage( const std::string_view value )
  {
    using Stage = doggo::cook::ShaderStage;
    if ( value == "vert" )
    {
      return Stage::Vertex;
    }

    if ( value == "tess_ctrl" )
    {
      return Stage::TessellationControl;
    }

    if ( value == "tess_eval" )
    {
      return Stage::TessellationEvaluation;
    }

    if ( value == "geom" )
    {
      return Stage::Geometry;
    }

    if ( value == "frag" )
    {
      return Stage::Fragment;
    }

    if ( value == "comp" )
    {
      return Stage::Compute;
    }

    throw std::runtime_error{ "Unknown UAM shader stage: " + std::string{ value } };
  }

  [[nodiscard]] ShaderProgramManifest parseProgramManifest( const std::filesystem::path & path )
  {
    const std::vector<std::vector<std::string>> lines = readTokenLines( path );
    if ( lines.empty() || lines.front().size() != 2 || lines.front()[ 0 ] != "doggo_shader_program" )
    {
      throw std::runtime_error{ path.string() + ": expected 'doggo_shader_program <version>'" };
    }

    ShaderProgramManifest manifest;
    manifest.version = parseUnsigned( lines.front()[ 1 ], path.string() );
    if ( manifest.version != 1 )
    {
      throw std::runtime_error{
          path.string() + ": unsupported shader program manifest version " + std::to_string( manifest.version ) };
    }

    std::set<doggo::cook::ShaderStage> stages;
    std::set<std::string>              variants;
    for ( std::size_t index = 1; index < lines.size(); ++index )
    {
      const std::vector<std::string> & tokens = lines[ index ];
      if ( tokens[ 0 ] == "program" )
      {
        if ( tokens.size() != 2 || !manifest.program.empty() )
        {
          throw std::runtime_error{ path.string() + ": exactly one 'program <name>' entry is required" };
        }

        if ( !isLowerSnakeIdentifier( tokens[ 1 ] ) )
        {
          throw std::runtime_error{ path.string() + ": program name must be lower_snake_case" };
        }

        manifest.program = tokens[ 1 ];
      }
      else if ( tokens[ 0 ] == "stage" )
      {
        if ( tokens.size() != 3 )
        {
          throw std::runtime_error{ path.string() + ": stage entries require 'stage <uam-stage> <source>'" };
        }

        const doggo::cook::ShaderStage stage = parseShaderStage( tokens[ 1 ] );
        if ( !stages.insert( stage ).second )
        {
          throw std::runtime_error{
              path.string() + ": duplicate " + std::string{ doggo::cook::getShaderStageName( stage ) } + " stage" };
        }

        std::filesystem::path source = path.parent_path() / tokens[ 2 ];
        if ( !std::filesystem::is_regular_file( source ) )
        {
          throw std::runtime_error{ path.string() + ": shader source not found: " + source.string() };
        }

        manifest.sources.push_back( ShaderSource{ .stage = stage, .path = std::move( source ) } );
      }
      else if ( tokens[ 0 ] == "variant" )
      {
        if ( tokens.size() < 2 || !isLowerSnakeIdentifier( tokens[ 1 ] ) )
        {
          throw std::runtime_error{ path.string() + ": variants require a lower_snake_case name" };
        }

        ShaderVariant variant{ .name = tokens[ 1 ], .defines = {} };
        if ( !variants.insert( variant.name ).second )
        {
          throw std::runtime_error{ path.string() + ": duplicate variant: " + variant.name };
        }

        std::set<std::string> defineNames;
        for ( std::size_t tokenIndex = 2; tokenIndex < tokens.size(); ++tokenIndex )
        {
          const std::string & token     = tokens[ tokenIndex ];
          const std::size_t   separator = token.find( '=' );
          const std::string   name      = token.substr( 0, separator );
          const std::string   value     = separator == std::string::npos ? "1" : token.substr( separator + 1 );
          if ( !isIdentifier( name ) ||
               value.empty() ||
               !std::ranges::all_of( value,
                                     []( const unsigned char character )
                                     {
                                       return std::isalnum( character ) != 0 ||
                                              character == '_' ||
                                              character == '+' ||
                                              character == '-' ||
                                              character == '.';
                                     } ) )
          {
            throw std::runtime_error{ path.string() + ": invalid variant define: " + token };
          }

          if ( !defineNames.insert( name ).second )
          {
            throw std::runtime_error{ path.string() + ": duplicate variant define: " + name };
          }

          variant.defines.push_back( ShaderDefine{ .name = name, .value = value } );
        }

        std::ranges::sort( variant.defines, {}, &ShaderDefine::name );
        manifest.variants.push_back( std::move( variant ) );
      }
      else
      {
        throw std::runtime_error{ path.string() + ": unknown manifest entry: " + tokens[ 0 ] };
      }
    }

    if ( manifest.program.empty() || manifest.sources.empty() )
    {
      throw std::runtime_error{ path.string() + ": a program and at least one stage are required" };
    }

    if ( manifest.variants.empty() )
    {
      manifest.variants.push_back( ShaderVariant{ .name = "default", .defines = {} } );
    }

    const bool hasCompute  = stages.contains( doggo::cook::ShaderStage::Compute );
    const bool hasGraphics = stages.size() > ( hasCompute ? 1u : 0u );

    if ( hasCompute && hasGraphics )
    {
      throw std::runtime_error{ path.string() + ": compute and graphics stages cannot share a program" };
    }

    if ( !hasCompute && ( !stages.contains( doggo::cook::ShaderStage::Vertex ) ||
                          !stages.contains( doggo::cook::ShaderStage::Fragment ) ) )
    {
      throw std::runtime_error{ path.string() + ": graphics programs require vertex and fragment stages" };
    }

    std::ranges::sort( manifest.sources, {}, &ShaderSource::stage );
    std::ranges::sort( manifest.variants, {}, &ShaderVariant::name );
    return manifest;
  }

  [[nodiscard]] std::string stripComments( const std::string_view source )
  {
    enum class State : std::uint8_t
    {
      Code,
      LineComment,
      BlockComment,
      String,
    };

    auto        state = State::Code;
    std::string result;
    result.reserve( source.size() );

    for ( std::size_t index = 0; index < source.size(); ++index )
    {
      const char current = source[ index ];
      const char next    = index + 1 < source.size() ? source[ index + 1 ] : '\0';

      if ( state == State::Code && current == '/' && next == '/' )
      {
        state = State::LineComment;
        result.append( "  " );
        ++index;
      }
      else if ( state == State::Code && current == '/' && next == '*' )
      {
        state = State::BlockComment;
        result.append( "  " );
        ++index;
      }
      else if ( state == State::LineComment && current == '\n' )
      {
        state = State::Code;
        result.push_back( current );
      }
      else if ( state == State::BlockComment && current == '*' && next == '/' )
      {
        state = State::Code;
        result.append( "  " );
        ++index;
      }
      else if ( state == State::Code && current == '"' )
      {
        state = State::String;
        result.push_back( current );
      }
      else if ( state == State::String && current == '"' && ( index == 0 || source[ index - 1 ] != '\\' ) )
      {
        state = State::Code;
        result.push_back( current );
      }
      else if ( state == State::Code || state == State::String )
      {
        result.push_back( current );
      }
      else
      {
        result.push_back( current == '\n' ? '\n' : ' ' );
      }
    }

    if ( state == State::BlockComment )
    {
      throw std::runtime_error{ "Unterminated block comment in shader source" };
    }

    return result;
  }

  [[nodiscard]] BindingKind classifyShaderBinding( const std::string_view storage, const std::string_view type )
  {
    if ( storage == "buffer" )
    {
      return BindingKind::StorageBuffer;
    }

    if ( type.starts_with( "sampler" ) || type.starts_with( "isampler" ) || type.starts_with( "usampler" ) )
    {
      return BindingKind::Sampler;
    }

    if ( type.starts_with( "image" ) || type.starts_with( "iimage" ) || type.starts_with( "uimage" ) )
    {
      return BindingKind::Image;
    }

    return BindingKind::UniformBuffer;
  }

  void validateShaderBindings( const std::filesystem::path & sourcePath, const std::string & source,
                               const ShaderAbi & abi )
  {
    const std::string stripped = stripComments( source );
    const std::regex  bindingPattern{
        R"(layout\s*\([^)]*binding\s*=\s*([A-Za-z_][A-Za-z0-9_]*|[0-9]+)[^)]*\)\s*((readonly|writeonly|coherent|volatile|restrict)\s+)*(uniform|buffer)\s+([A-Za-z_][A-Za-z0-9_]*))" };
    const std::regex anyBindingPattern{ R"(\bbinding\s*=)" };

    std::map<std::string, const Binding *> bindingByMacro;
    for ( const Binding & binding : abi.bindings )
    {
      bindingByMacro.emplace( binding.macro, &binding );
    }

    std::set<std::pair<BindingKind, std::uint32_t>> used;
    std::size_t                                     parsedCount = 0;

    for ( auto iterator = std::sregex_iterator{ stripped.begin(), stripped.end(), bindingPattern };
          iterator != std::sregex_iterator{};
          ++iterator )
    {
      ++parsedCount;
      const std::string macro = ( *iterator )[ 1 ].str();

      if ( std::isdigit( static_cast<unsigned char>( macro.front() ) ) != 0 )
      {
        throw std::runtime_error{ sourcePath.string() +
                                  ": numeric shader bindings are forbidden; use a DOGGO ABI macro" };
      }

      const auto found = bindingByMacro.find( macro );
      if ( found == bindingByMacro.end() )
      {
        throw std::runtime_error{ sourcePath.string() + ": binding macro is not declared by the shader ABI: " + macro };
      }

      const BindingKind actual = classifyShaderBinding( ( *iterator )[ 4 ].str(), ( *iterator )[ 5 ].str() );
      if ( actual != found->second->kind )
      {
        throw std::runtime_error{ sourcePath.string() +
                                  ": " +
                                  macro +
                                  " names a " +
                                  std::string{ bindingKindToken( found->second->kind ) } +
                                  " slot but is used by a " +
                                  std::string{ bindingKindToken( actual ) } +
                                  " declaration" };
      }

      if ( !used.emplace( actual, found->second->slot ).second )
      {
        throw std::runtime_error{
            sourcePath.string() + ": binding slot is declared more than once in this stage: " + macro };
      }
    }

    const auto bindingCount = static_cast<std::size_t>( std::distance(
        std::sregex_iterator{ stripped.begin(), stripped.end(), anyBindingPattern }, std::sregex_iterator{} ) );
    if ( parsedCount != bindingCount )
    {
      throw std::runtime_error{
          sourcePath.string() +
          ": unsupported binding declaration; keep layout(binding=...) and its resource declaration together" };
    }
  }

  [[nodiscard]] StageInterface parseStageInterface( const std::filesystem::path & sourcePath,
                                                    const std::string &           source )
  {
    const std::string stripped = stripComments( source );
    const std::regex  interfacePattern{
        R"(layout\s*\([^)]*location\s*=\s*([0-9]+)[^)]*\)\s*((flat|smooth|noperspective|centroid|sample|invariant)\s+)*(in|out)\s+([A-Za-z_][A-Za-z0-9_]*))" };

    StageInterface result;
    for ( auto iterator = std::sregex_iterator{ stripped.begin(), stripped.end(), interfacePattern };
          iterator != std::sregex_iterator{};
          ++iterator )
    {
      const std::uint32_t location  = parseUnsigned( ( *iterator )[ 1 ].str(), sourcePath.string() );
      const std::string   direction = ( *iterator )[ 4 ].str();
      const std::string   type      = ( *iterator )[ 5 ].str();
      auto &              entries   = direction == "in" ? result.inputs : result.outputs;

      if ( !entries.emplace( location, type ).second )
      {
        throw std::runtime_error{ sourcePath.string() +
                                  ": interface location " +
                                  std::to_string( location ) +
                                  " is declared more than once for " +
                                  direction };
      }
    }

    return result;
  }

  void validateStageInterfaces( const ShaderProgramManifest &                              manifest,
                                const std::map<doggo::cook::ShaderStage, StageInterface> & interfaces )
  {
    if ( manifest.sources.front().stage == doggo::cook::ShaderStage::Compute )
    {
      return;
    }

    for ( std::size_t index = 1; index < manifest.sources.size(); ++index )
    {
      const ShaderSource & previous = manifest.sources[ index - 1 ];
      const ShaderSource & current  = manifest.sources[ index ];
      const auto &         outputs  = interfaces.at( previous.stage ).outputs;
      const auto &         inputs   = interfaces.at( current.stage ).inputs;

      for ( const auto & [ location, inputType ] : inputs )
      {
        const auto found = outputs.find( location );
        if ( found == outputs.end() )
        {
          throw std::runtime_error{ current.path.string() +
                                    ": location " +
                                    std::to_string( location ) +
                                    " is not produced by the preceding " +
                                    std::string{ doggo::cook::getShaderStageName( previous.stage ) } +
                                    " stage" };
        }

        if ( found->second != inputType )
        {
          throw std::runtime_error{ current.path.string() +
                                    ": location " +
                                    std::to_string( location ) +
                                    " expects " +
                                    inputType +
                                    " but the preceding stage produces " +
                                    found->second };
        }
      }
    }
  }

  [[nodiscard]] std::string buildCookSource( const std::string & source, const ShaderAbi & abi,
                                             const ShaderVariant & variant )
  {
    std::size_t position   = 0;
    std::size_t lineNumber = 1;
    std::size_t insertAt   = std::string::npos;

    while ( position <= source.size() )
    {
      const std::size_t lineEnd = source.find( '\n', position );
      const std::size_t length  = lineEnd == std::string::npos ? source.size() - position : lineEnd - position;
      const std::string line    = trim( source.substr( position, length ) );

      if ( !line.empty() )
      {
        if ( !line.starts_with( "#version" ) )
        {
          throw std::runtime_error{ "The first non-empty shader line must be #version" };
        }

        insertAt = lineEnd == std::string::npos ? source.size() : lineEnd + 1;
        break;
      }

      if ( lineEnd == std::string::npos )
      {
        break;
      }

      position = lineEnd + 1;
      ++lineNumber;
    }

    if ( insertAt == std::string::npos )
    {
      throw std::runtime_error{ "Shader source has no #version directive" };
    }

    std::ostringstream preamble;
    preamble << "#define DOGGO_SHADER_ABI_VERSION " << abi.version << '\n';

    for ( const Binding & binding : abi.bindings )
    {
      preamble << "#define " << binding.macro << ' ' << binding.slot << '\n';
    }

    for ( const ShaderDefine & define : variant.defines )
    {
      preamble << "#define " << define.name << ' ' << define.value << '\n';
    }

    preamble << "#line " << lineNumber + 1 << '\n';

    std::string result;
    result.reserve( source.size() + static_cast<std::size_t>( preamble.tellp() ) );
    result.append( source.substr( 0, insertAt ) );
    result.append( preamble.str() );
    result.append( source.substr( insertAt ) );
    return result;
  }

  [[nodiscard]] std::string_view stageSuffix( const doggo::cook::ShaderStage stage ) noexcept
  {
    using Stage = doggo::cook::ShaderStage;
    switch ( stage )
    {
      case Stage::Vertex:
        return "vsh";
      case Stage::TessellationControl:
        return "tcsh";
      case Stage::TessellationEvaluation:
        return "tesh";
      case Stage::Geometry:
        return "gsh";
      case Stage::Fragment:
        return "fsh";
      case Stage::Compute:
        return "csh";
    }
    return "unknown";
  }

  [[nodiscard]] std::string shaderFilename( const ShaderProgramManifest & manifest, const ShaderVariant & variant,
                                            const doggo::cook::ShaderStage stage )
  {
    std::string filename = manifest.program;
    if ( variant.name != "default" )
    {
      filename += '_' + variant.name;
    }

    filename += '_' + std::string{ stageSuffix( stage ) } + ".dksh";
    return filename;
  }

  [[nodiscard]] std::string quoteManifestValue( const std::string_view value )
  {
    std::string result = "\"";
    for ( const char character : value )
    {
      if ( character == '\\' || character == '"' )
      {
        result.push_back( '\\' );
      }

      result.push_back( character );
    }

    result.push_back( '"' );
    return result;
  }

  [[nodiscard]] std::string makeCookedManifest( const ShaderProgramManifest & manifest, const ShaderAbi & abi,
                                                const std::string_view             compilerVersion,
                                                const std::vector<PendingShader> & shaders )
  {
    std::ostringstream output;
    output << "doggo_cooked_shader_manifest 1\n"
           << "shader_abi " << abi.version << "\n"
           << "program " << manifest.program << "\n"
           << "compiler " << quoteManifestValue( collapseWhitespace( compilerVersion ) ) << "\n";

    for ( const PendingShader & shader : shaders )
    {
      output << "stage " << shader.variant << ' ' << doggo::cook::getShaderStageName( shader.stage ) << ' '
             << shader.destination.filename().generic_string() << ' ' << shader.size << " 0x" << std::hex
             << std::uppercase << std::setw( 16 ) << std::setfill( '0' ) << shader.hash << std::dec << '\n';
    }

    return output.str();
  }

  [[nodiscard]] std::string makeBindingHeader( const ShaderAbi & abi )
  {
    std::ostringstream output;
    output << "#pragma once\n\n"
              "#include <cstdint>\n\n"
              "namespace doggo::render::shader_binding\n"
              "{\n"
           << "  inline constexpr std::uint32_t AbiVersion = " << abi.version << ";\n";

    for ( constexpr std::array bindingKinds = { BindingKind::UniformBuffer, BindingKind::StorageBuffer,
                                                BindingKind::Sampler, BindingKind::Image };
          const BindingKind    kind : bindingKinds )
    {
      const bool hasKind = std::ranges::any_of( abi.bindings,
                                                [ kind ]( const Binding & binding )
                                                {
                                                  return binding.kind == kind;
                                                } );
      if ( !hasKind )
      {
        continue;
      }

      output << "\n  namespace " << bindingKindToken( kind ) << "\n  {\n";
      for ( const Binding & binding : abi.bindings )
      {
        if ( binding.kind == kind )
        {
          output << "    inline constexpr std::uint32_t " << pascalCase( binding.name ) << " = " << binding.slot
                 << ";\n";
        }
      }

      output << "  }  // namespace " << bindingKindToken( kind ) << '\n';
    }

    output << "}  // namespace doggo::render::shader_binding\n";
    return output.str();
  }

  [[nodiscard]] std::string shellQuote( const std::filesystem::path & path )
  {
    const std::string value = path.string();
#if defined( _WIN32 )
    std::string result = "\"";
    for ( const char character : value )
    {
      if ( character == '"' )
      {
        result.push_back( '\\' );
      }

      result.push_back( character );
    }

    result.push_back( '"' );
    return result;
#else
    std::string result = "'";
    for ( const char character : value )
    {
      if ( character == '\'' )
      {
        result.append( "'\\''" );
      }
      else
      {
        result.push_back( character );
      }
    }

    result.push_back( '\'' );
    return result;
#endif
  }

  [[nodiscard]] std::filesystem::path makeDiagnosticPath( const std::string_view stem )
  {
    const auto            nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    std::error_code       error;
    std::filesystem::path directory = std::filesystem::temp_directory_path( error );

    if ( error )
    {
      directory = std::filesystem::current_path() / ".doggo-cook-temp";
      std::filesystem::create_directories( directory );
    }

    return directory / ( "doggo-cook-" + std::string{ stem } + '-' + std::to_string( nonce ) + ".txt" );
  }

  [[nodiscard]] int runRedirected( const std::string & command, const std::filesystem::path & diagnostics )
  {
    const std::string redirected = command + " > " + shellQuote( diagnostics ) + " 2>&1";
    return std::system( redirected.c_str() );
  }
}  // namespace

namespace doggo::cook
{
  std::string_view getShaderStageName( const ShaderStage stage ) noexcept
  {
    switch ( stage )
    {
      case ShaderStage::Vertex:
        return "vert";
      case ShaderStage::TessellationControl:
        return "tess_ctrl";
      case ShaderStage::TessellationEvaluation:
        return "tess_eval";
      case ShaderStage::Geometry:
        return "geom";
      case ShaderStage::Fragment:
        return "frag";
      case ShaderStage::Compute:
        return "comp";
    }

    return "unknown";
  }

  UamCompiler::UamCompiler( std::filesystem::path executable )
      : mExecutable{ std::move( executable ) }
  {
    if ( mExecutable.empty() )
    {
      throw std::runtime_error{ "UAM executable path is empty" };
    }
  }

  std::string UamCompiler::version()
  {
    const std::filesystem::path diagnostics = makeDiagnosticPath( "uam-version" );
    const int                   result      = runRedirected( shellQuote( mExecutable ) + " -v", diagnostics );
    const std::string           output =
        std::filesystem::is_regular_file( diagnostics ) ? readText( diagnostics ) : std::string{};
    std::error_code ignored;
    std::filesystem::remove( diagnostics, ignored );

    if ( result != 0 )
    {
      throw std::runtime_error{ "Unable to query UAM version: " + collapseWhitespace( output ) };
    }

    if ( collapseWhitespace( output ).empty() )
    {
      throw std::runtime_error{ "UAM returned an empty version string" };
    }

    return collapseWhitespace( output );
  }

  void UamCompiler::compile( const ShaderStage stage, const std::filesystem::path & source,
                             const std::filesystem::path & output )
  {
    const std::filesystem::path diagnostics = output.string() + ".log";
    std::error_code             ignored;
    std::filesystem::remove( output, ignored );
    std::filesystem::remove( diagnostics, ignored );

    const std::string command = shellQuote( mExecutable ) +
                                " -o " +
                                shellQuote( output ) +
                                " -s " +
                                std::string{ getShaderStageName( stage ) } +
                                ' ' +
                                shellQuote( source );
    const int         result = runRedirected( command, diagnostics );
    const std::string compilerOutput =
        std::filesystem::is_regular_file( diagnostics ) ? readText( diagnostics ) : std::string{};
    std::filesystem::remove( diagnostics, ignored );

    if ( result != 0 || !std::filesystem::is_regular_file( output ) )
    {
      std::filesystem::remove( output, ignored );
      throw std::runtime_error{ "UAM failed for " + source.string() + ": " + collapseWhitespace( compilerOutput ) };
    }
  }

  void cookShaderProgram( const ShaderCookOptions & options, ShaderCompiler & compiler )
  {
    if ( options.manifest.empty() || options.abi.empty() || options.output_directory.empty() )
    {
      throw std::runtime_error{ "Shader cook options contain an empty path" };
    }

    const ShaderAbi             abi      = parseShaderAbi( options.abi );
    const ShaderProgramManifest manifest = parseProgramManifest( options.manifest );

    std::map<ShaderStage, std::string>    sourceText;
    std::map<ShaderStage, StageInterface> interfaces;

    for ( const ShaderSource & source : manifest.sources )
    {
      std::string text = readText( source.path );
      validateShaderBindings( source.path, text, abi );
      interfaces.emplace( source.stage, parseStageInterface( source.path, text ) );
      sourceText.emplace( source.stage, std::move( text ) );
    }

    validateStageInterfaces( manifest, interfaces );

    const std::string           compilerVersion  = compiler.version();
    const std::filesystem::path stagingDirectory = options.output_directory / ".doggo-cook" / manifest.program;
    std::error_code             ignored;
    std::filesystem::remove_all( stagingDirectory, ignored );
    std::filesystem::create_directories( stagingDirectory );

    try
    {
      std::vector<PendingShader> pending;
      for ( const ShaderVariant & variant : manifest.variants )
      {
        for ( const ShaderSource & source : manifest.sources )
        {
          const std::string           filename = shaderFilename( manifest, variant, source.stage );
          const std::filesystem::path stagedSource =
              stagingDirectory / ( variant.name + '_' + std::string{ stageSuffix( source.stage ) } + ".glsl" );
          const std::filesystem::path stagedOutput = stagingDirectory / filename;
          writeTextIfDifferent( stagedSource, buildCookSource( sourceText.at( source.stage ), abi, variant ) );
          compiler.compile( source.stage, stagedSource, stagedOutput );

          const std::vector<std::uint8_t> bytes = readBytes( stagedOutput );
          if ( bytes.empty() )
          {
            throw std::runtime_error{ "Shader compiler produced an empty DKSH file for " + source.path.string() };
          }

          pending.push_back( PendingShader{
              .variant     = variant.name,
              .stage       = source.stage,
              .temporary   = stagedOutput,
              .destination = options.output_directory / filename,
              .size        = bytes.size(),
              .hash        = fnv1a64( bytes ),
          } );
        }
      }

      for ( const PendingShader & shader : pending )
      {
        installIfDifferent( shader.temporary, shader.destination );
      }

      writeTextIfDifferent( options.output_directory / ( manifest.program + ".dsm" ),
                            makeCookedManifest( manifest, abi, compilerVersion, pending ) );
      std::filesystem::remove_all( stagingDirectory, ignored );
    }
    catch ( ... )
    {
      std::filesystem::remove_all( stagingDirectory, ignored );
      throw;
    }
  }

  void generateShaderBindingHeader( const std::filesystem::path & abi, const std::filesystem::path & output )
  {
    if ( abi.empty() || output.empty() )
    {
      throw std::runtime_error{ "Shader ABI input or output path is empty" };
    }

    writeTextIfDifferent( output, makeBindingHeader( parseShaderAbi( abi ) ) );
  }
}  // namespace doggo::cook
