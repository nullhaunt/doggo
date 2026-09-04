#include "LinearAllocator.hpp"

#include <algorithm>
#include <bit>
#include <memory>

#include "diagnostics/Assert.hpp"

namespace doggo::memory
{
    LinearAllocator::LinearAllocator( std::span<std::byte> memory ) noexcept
        : mMemory( memory )
    {
        DOGGO_ASSERT_MSG( !mMemory.empty(), "LinearAllocator requires non-empty backing memory." );
    }

    void * LinearAllocator::allocate( std::size_t size, std::size_t alignment ) noexcept
    {
        DOGGO_ASSERT_MSG( size > 0, "LinearAllocator cannot allocate zero bytes." );
        DOGGO_ASSERT_MSG( std::has_single_bit( alignment ), "LinearAllocator alignment must be a power of two." );

        if ( mMemory.empty() || size == 0 || !std::has_single_bit( alignment ) )
        {
            return nullptr;
        }

        DOGGO_ASSERT( mOffset <= mMemory.size() );

        std::byte * const unalignedAddress = mMemory.data() + mOffset;
        void *            alignedAddress   = unalignedAddress;
        std::size_t       remaining        = mMemory.size() - mOffset;

        if ( !std::align( alignment, size, alignedAddress, remaining ) )
        {
            return nullptr;
        }

        const auto * const alignedByteAddress  = static_cast<std::byte *>( alignedAddress );
        const auto         padding             = static_cast<std::size_t>( alignedByteAddress - unalignedAddress );
        mOffset                               += padding + size;
        mPeakUsage                             = std::max( mPeakUsage, mOffset );

        return alignedAddress;
    }

    void LinearAllocator::reset() noexcept
    {
        mOffset = 0;
    }

    std::size_t LinearAllocator::getCapacity() const noexcept
    {
        return mMemory.size();
    }

    std::size_t LinearAllocator::getUsed() const noexcept
    {
        return mOffset;
    }

    std::size_t LinearAllocator::getRemaining() const noexcept
    {
        return mMemory.size() - mOffset;
    }

    std::size_t LinearAllocator::getPeakUsage() const noexcept
    {
        return mPeakUsage;
    }
} // namespace doggo::memory
