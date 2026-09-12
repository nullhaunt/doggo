#include "ShaderCooker.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  class TemporaryDirectory final
  {
    public:
      TemporaryDirectory( const TemporaryDirectory & )             = delete;
      TemporaryDirectory( TemporaryDirectory && )                  = delete;
      TemporaryDirectory & operator=( const TemporaryDirectory & ) = delete;
      TemporaryDirectory & operator=( TemporaryDirectory && )      = delete;

      TemporaryDirectory()
      {
        const auto            nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        std::error_code       error;
        std::filesystem::path root = std::filesystem::temp_directory_path( error );

        if ( error )
        {
          root = std::filesystem::current_path() / ".doggo-cook-tests";
        }

        mPath = root / ( "doggo-cook-tests-" + std::to_string( nonce ) );
        std::filesystem::create_directories( mPath );
      }

      ~TemporaryDirectory()
      {
        std::error_code ignored;
        std::filesystem::remove_all( mPath, ignored );
      }

      [[nodiscard]] const std::filesystem::path & path() const noexcept
      {
        return mPath;
      }

    private:
      std::filesystem::path mPath;
  };

  class FakeCompiler final : public doggo::cook::ShaderCompiler
  {
    public:
      bool fail_fragment = false;

      [[nodiscard]] std::string version() override
      {
        return "fake-uam 1.0\n";
      }

      void compile( const doggo::cook::ShaderStage stage, const std::filesystem::path & source,
                    const std::filesystem::path & output ) override
      {
        if ( fail_fragment && stage == doggo::cook::ShaderStage::Fragment )
        {
          throw std::runtime_error{ "intentional compiler failure" };
        }

        std::ifstream input{ source, std::ios::binary };
        if ( !input )
        {
          throw std::runtime_error{ "fake compiler could not open input" };
        }

        const std::string text{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{} };

        std::ofstream result{ output, std::ios::binary | std::ios::trunc };
        result << "FAKE-DKSH\n" << doggo::cook::getShaderStageName( stage ) << '\n' << text;

        if ( !result )
        {
          throw std::runtime_error{ "fake compiler could not write output" };
        }
      }
  };

  void writeText( const std::filesystem::path & path, const std::string_view text )
  {
    std::filesystem::create_directories( path.parent_path() );
    std::ofstream stream{ path, std::ios::binary | std::ios::trunc };
    stream.write( text.data(), static_cast<std::streamsize>( text.size() ) );

    if ( !stream )
    {
      throw std::runtime_error{ "test could not write " + path.string() };
    }
  }

  [[nodiscard]] std::string readText( const std::filesystem::path & path )
  {
    std::ifstream stream{ path, std::ios::binary };
    if ( !stream )
    {
      throw std::runtime_error{ "test could not read " + path.string() };
    }

    return std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{} };
  }

  void require( const bool condition, const std::string_view message )
  {
    if ( !condition )
    {
      throw std::runtime_error{ std::string{ message } };
    }
  }

  template <typename Function>
  void requireFailure( Function && function, const std::string_view expected )
  {
    try
    {
      std::invoke( std::forward<Function>( function ) );
    }
    catch ( const std::exception & error )
    {
      require( std::string_view{ error.what() }.find( expected ) != std::string_view::npos,
               "failure did not contain the expected diagnostic" );
      return;
    }

    throw std::runtime_error{ "operation unexpectedly succeeded" };
  }

  struct Fixture final
  {
      TemporaryDirectory             temporary;
      std::filesystem::path          abi;
      std::filesystem::path          manifest;
      std::filesystem::path          vertex;
      std::filesystem::path          fragment;
      std::filesystem::path          output;
      doggo::cook::ShaderCookOptions options;

      Fixture()
      {
        abi      = temporary.path() / "doggo_shader_abi.dsa";
        manifest = temporary.path() / "test.shader";
        vertex   = temporary.path() / "test_vsh.glsl";
        fragment = temporary.path() / "test_fsh.glsl";
        output   = temporary.path() / "cooked";
        options  = { .manifest = manifest, .abi = abi, .output_directory = output };

        writeText( abi,
                   "doggo_shader_abi 1\n"
                   "ubo 0 frame_view\n"
                   "sampler 0 base_color\n" );
        writeText( manifest,
                   "doggo_shader_program 1\n"
                   "program test_program\n"
                   "stage vert test_vsh.glsl\n"
                   "stage frag test_fsh.glsl\n"
                   "variant default\n"
                   "variant skinned DOGGO_SKINNED=1\n" );
        writeText( vertex,
                   "#version 460\n"
                   "layout (binding = DOGGO_UBO_FRAME_VIEW, std140) uniform FrameView { mat4 projection; } frame;\n"
                   "layout (location = 0) out vec4 outColor;\n"
                   "void main() { gl_Position = frame.projection * vec4(0.0); outColor = vec4(1.0); }\n" );
        writeText( fragment,
                   "#version 460\n"
                   "layout (binding = DOGGO_SAMPLER_BASE_COLOR) uniform sampler2D baseColor;\n"
                   "layout (location = 0) in vec4 inColor;\n"
                   "layout (location = 0) out vec4 outColor;\n"
                   "void main() { outColor = inColor * texture(baseColor, vec2(0.0)); }\n" );
      }
  };

  void testSuccessfulCookAndDeterminism()
  {
    const Fixture fixture;
    FakeCompiler  compiler;
    doggo::cook::cookShaderProgram( fixture.options, compiler );

    const std::filesystem::path vertex         = fixture.output / "test_program_vsh.dksh";
    const std::filesystem::path fragment       = fixture.output / "test_program_fsh.dksh";
    const std::filesystem::path skinnedVertex  = fixture.output / "test_program_skinned_vsh.dksh";
    const std::filesystem::path cookedManifest = fixture.output / "test_program.dsm";
    require( std::filesystem::is_regular_file( vertex ), "default vertex output is missing" );
    require( std::filesystem::is_regular_file( fragment ), "default fragment output is missing" );
    require( std::filesystem::is_regular_file( skinnedVertex ), "variant vertex output is missing" );

    const std::string manifestText = readText( cookedManifest );
    require( manifestText.find( "shader_abi 1" ) != std::string::npos, "cooked manifest omits ABI version" );
    require( manifestText.find( "compiler \"fake-uam 1.0\"" ) != std::string::npos,
             "compiler version was not normalized" );
    require( readText( skinnedVertex ).find( "#define DOGGO_SKINNED 1" ) != std::string::npos,
             "variant define was not injected" );

    const std::filesystem::file_time_type vertexTime   = std::filesystem::last_write_time( vertex );
    const std::filesystem::file_time_type manifestTime = std::filesystem::last_write_time( cookedManifest );
    doggo::cook::cookShaderProgram( fixture.options, compiler );
    require( std::filesystem::last_write_time( vertex ) == vertexTime, "unchanged DKSH was rewritten" );
    require( std::filesystem::last_write_time( cookedManifest ) == manifestTime,
             "unchanged cooked manifest was rewritten" );
  }

  void testBindingHeader()
  {
    const Fixture               fixture;
    const std::filesystem::path header = fixture.temporary.path() / "render_ShaderBindings.hpp";
    doggo::cook::generateShaderBindingHeader( fixture.abi, header );
    const std::string text = readText( header );
    require( text.find( "AbiVersion = 1" ) != std::string::npos, "generated header omits ABI version" );
    require( text.find( "FrameView = 0" ) != std::string::npos, "generated header omits UBO binding" );
    require( text.find( "BaseColor = 0" ) != std::string::npos, "generated header omits sampler binding" );
  }

  void testNumericBindingRejected()
  {
    const Fixture fixture;
    writeText( fixture.fragment,
               "#version 460\n"
               "layout (binding = 0) uniform sampler2D baseColor;\n"
               "layout (location = 0) in vec4 inColor;\n"
               "layout (location = 0) out vec4 outColor;\n"
               "void main() { outColor = inColor; }\n" );

    FakeCompiler compiler;
    requireFailure(
        [ & ]
        {
          doggo::cook::cookShaderProgram( fixture.options, compiler );
        },
        "numeric shader bindings are forbidden" );
  }

  void testBindingKindMismatchRejected()
  {
    const Fixture fixture;
    writeText( fixture.fragment,
               "#version 460\n"
               "layout (binding = DOGGO_UBO_FRAME_VIEW) uniform sampler2D baseColor;\n"
               "layout (location = 0) in vec4 inColor;\n"
               "layout (location = 0) out vec4 outColor;\n"
               "void main() { outColor = inColor; }\n" );

    FakeCompiler compiler;
    requireFailure(
        [ & ]
        {
          doggo::cook::cookShaderProgram( fixture.options, compiler );
        },
        "names a ubo slot but is used by a sampler" );
  }

  void testInterfaceMismatchRejected()
  {
    const Fixture fixture;
    writeText( fixture.fragment,
               "#version 460\n"
               "layout (location = 0) in vec3 inColor;\n"
               "layout (location = 0) out vec4 outColor;\n"
               "void main() { outColor = vec4(inColor, 1.0); }\n" );

    FakeCompiler compiler;
    requireFailure(
        [ & ]
        {
          doggo::cook::cookShaderProgram( fixture.options, compiler );
        },
        "expects vec3" );
  }

  void testDuplicateAbiSlotRejected()
  {
    const Fixture fixture;
    writeText( fixture.abi,
               "doggo_shader_abi 1\n"
               "ubo 0 frame_view\n"
               "ubo 0 material\n" );
    requireFailure(
        [ & ]
        {
          doggo::cook::generateShaderBindingHeader( fixture.abi,
                                                    fixture.temporary.path() / "render_ShaderBindings.hpp" );
        },
        "duplicate ubo binding 0" );
  }

  void testCompilerFailurePreservesOutputs()
  {
    const Fixture fixture;
    FakeCompiler  compiler;
    doggo::cook::cookShaderProgram( fixture.options, compiler );
    const std::filesystem::path vertex = fixture.output / "test_program_vsh.dksh";
    const std::string           before = readText( vertex );

    compiler.fail_fragment = true;
    requireFailure(
        [ & ]
        {
          doggo::cook::cookShaderProgram( fixture.options, compiler );
        },
        "intentional compiler failure" );

    require( readText( vertex ) == before, "failed cook changed the previous valid output" );
  }
}  // namespace

int main()
{
  const std::vector<std::pair<std::string_view, std::function<void()>>> tests = {
      { "successful cook and determinism", testSuccessfulCookAndDeterminism },
      { "binding header", testBindingHeader },
      { "numeric binding rejection", testNumericBindingRejected },
      { "binding-kind mismatch rejection", testBindingKindMismatchRejected },
      { "interface mismatch rejection", testInterfaceMismatchRejected },
      { "duplicate ABI slot rejection", testDuplicateAbiSlotRejected },
      { "compiler failure preservation", testCompilerFailurePreservesOutputs },
  };

  std::size_t failures = 0;
  for ( const auto & [ name, test ] : tests )
  {
    try
    {
      test();
      std::cout << "[PASS] " << name << '\n';
    }
    catch ( const std::exception & error )
    {
      ++failures;
      std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
    }
  }

  if ( failures != 0 )
  {
    std::cerr << failures << " test(s) failed.\n";
    return 1;
  }

  return 0;
}
