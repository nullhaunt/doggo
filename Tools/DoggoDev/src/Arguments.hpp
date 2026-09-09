#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace doggo::devtool
{
  struct Options final
  {
      std::filesystem::path    nro;
      std::string              switch_address;
      std::string              remote_path;
      std::vector<std::string> application_arguments;
  };

  void printUsage();

  [[nodiscard]] std::optional<Options> parseOptions( int argc, char * const argv[] );
}  // namespace doggo::devtool
