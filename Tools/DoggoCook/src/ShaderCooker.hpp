#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace doggo::cook
{
  enum class ShaderStage : std::uint8_t
  {
    Vertex,
    TessellationControl,
    TessellationEvaluation,
    Geometry,
    Fragment,
    Compute,
  };

  [[nodiscard]] std::string_view getShaderStageName( ShaderStage stage ) noexcept;

  class ShaderCompiler
  {
    public:
      ShaderCompiler( const ShaderCompiler & )             = delete;
      ShaderCompiler( ShaderCompiler && )                  = delete;
      ShaderCompiler & operator=( const ShaderCompiler & ) = delete;
      ShaderCompiler & operator=( ShaderCompiler && )      = delete;

      ShaderCompiler()          = default;
      virtual ~ShaderCompiler() = default;

      [[nodiscard]] virtual std::string version()                                       = 0;
      virtual void                      compile( ShaderStage stage, const std::filesystem::path & source,
                                                 const std::filesystem::path & output ) = 0;
  };

  class UamCompiler final : public ShaderCompiler
  {
    public:
      explicit UamCompiler( std::filesystem::path executable );

      [[nodiscard]] std::string version() override;
      void                      compile( ShaderStage stage, const std::filesystem::path & source,
                                         const std::filesystem::path & output ) override;

    private:
      std::filesystem::path mExecutable;
  };

  struct ShaderCookOptions final
  {
      std::filesystem::path manifest;
      std::filesystem::path abi;
      std::filesystem::path output_directory;
  };

  void cookShaderProgram( const ShaderCookOptions & options, ShaderCompiler & compiler );
  void generateShaderBindingHeader( const std::filesystem::path & abi, const std::filesystem::path & output );
}  // namespace doggo::cook
