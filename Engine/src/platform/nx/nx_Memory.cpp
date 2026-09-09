#include "doggo/platform/nx/nx_Memory.hpp"

#include <switch.h>

#include <algorithm>

namespace doggo::platform::nx
{
  std::uint32_t queryMemoryReport( MemoryReport & report ) noexcept
  {
    std::uint64_t processTotal = 0;
    std::uint64_t processUsed  = 0;
    std::uint64_t heapRegion   = 0;

    Result result = svcGetInfo( &processTotal, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0 );
    if ( R_FAILED( result ) )
    {
      return result;
    }

    result = svcGetInfo( &processUsed, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0 );
    if ( R_FAILED( result ) )
    {
      return result;
    }

    result = svcGetInfo( &heapRegion, InfoType_HeapRegionSize, CUR_PROCESS_HANDLE, 0 );
    if ( R_FAILED( result ) )
    {
      return result;
    }

    report.process_total_bytes = processTotal;
    report.process_used_bytes  = processUsed;
    report.process_free_bytes  = processTotal - std::min( processTotal, processUsed );
    report.heap_region_bytes   = heapRegion;

    return 0;
  }
}  // namespace doggo::platform::nx
