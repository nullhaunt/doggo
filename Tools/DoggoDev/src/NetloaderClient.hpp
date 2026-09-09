#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace doggo::devtool
{
  struct UploadResult final
  {
      std::uint64_t source_bytes     = 0;
      std::uint64_t compressed_bytes = 0;
  };

  class NetloaderClient final
  {
    public:
      NetloaderClient( std::string host, std::uint16_t port );

      [[nodiscard]] UploadResult upload( const std::filesystem::path &    nro,
                                         std::string_view                 remotePath,
                                         const std::vector<std::string> & applicationArguments ) const;

    private:
      std::string   mHost;
      std::uint16_t mPort;
  };
}  // namespace doggo::devtool
