#pragma once

#include <cstdint>

namespace doggo::platform::nx
{
  struct MemoryReport final
  {
      std::uint64_t process_total_bytes = 0;
      std::uint64_t process_used_bytes  = 0;
      std::uint64_t process_free_bytes  = 0;
      std::uint64_t heap_region_bytes   = 0;
  };

  // Returns a libnx Result value. Zero indicates success.
  [[nodiscard]] std::uint32_t queryMemoryReport( MemoryReport & report ) noexcept;
}  // namespace doggo::platform::nx
