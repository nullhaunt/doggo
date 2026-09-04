#include "DekoMemoryArena.hpp"

#include "diagnostics/Assert.hpp"

namespace doggo::platform::nx::deko
{
    bool DekoMemoryArena::initialize( DkDevice device, std::uint32_t capacity, std::uint32_t flags ) noexcept
    {
        DOGGO_ASSERT( !mMemory );
        DOGGO_ASSERT( capacity > 0 );
        DOGGO_ASSERT( capacity % DK_MEMBLOCK_ALIGNMENT == 0 );

        if ( mMemory || capacity == 0 || capacity % DK_MEMBLOCK_ALIGNMENT != 0 )
        {
            return false;
        }

        mMemory = dk::UniqueMemBlock{ dk::MemBlockMaker{ device, capacity }.setFlags( flags ).create() };

        if ( !mMemory )
        {
            return false;
        }

        mCapacity = capacity;
        mOffset   = 0;

        return true;
    }

    std::optional<DekoMemorySlice> DekoMemoryArena::allocate( std::uint32_t size, std::uint32_t alignment ) noexcept
    {
        DOGGO_ASSERT( size > 0 );
        DOGGO_ASSERT( std::has_single_bit( alignment ) );

        if ( !mMemory || size == 0 || !std::has_single_bit( alignment ) )
        {
            return std::nullopt;
        }

        const auto alignmentMask = static_cast<std::uint64_t>( alignment - 1 );
        const auto alignedOffset = ( static_cast<std::uint64_t>( mOffset ) + alignmentMask ) & ~alignmentMask;

        if ( alignedOffset > mCapacity || size > mCapacity - alignedOffset )
        {
            return std::nullopt;
        }

        DekoMemorySlice slice{ .mOffset = static_cast<std::uint32_t>( alignedOffset ), .mSize = size };
        mOffset = slice.mOffset + slice.mSize;

        return slice;
    }

    DkMemBlock DekoMemoryArena::getMemoryBlock() const noexcept
    {
        return mMemory;
    }

    void * DekoMemoryArena::getCpuAddress( const DekoMemorySlice & slice ) noexcept
    {
        DOGGO_ASSERT( slice.mOffset <= mCapacity );
        DOGGO_ASSERT( mOffset <= mCapacity - slice.mOffset );

        return static_cast<std::byte *>( mMemory.getCpuAddr() ) + slice.mOffset;
    }

    DkGpuAddr DekoMemoryArena::getGpuAddress( const DekoMemorySlice & slice ) const noexcept
    {
        DOGGO_ASSERT( slice.mOffset <= mCapacity );
        DOGGO_ASSERT( mOffset <= mCapacity - slice.mOffset );

        const DkMemBlock memory = mMemory;
        return dkMemBlockGetGpuAddr( memory ) + slice.mOffset;
    }

    std::uint32_t DekoMemoryArena::getCapacity() const noexcept
    {
        return mCapacity;
    }

    std::uint32_t DekoMemoryArena::getUsed() const noexcept
    {
        return mOffset;
    }

    std::uint32_t DekoMemoryArena::getRemaining() const noexcept
    {
        return mCapacity - mOffset;
    }
} // namespace doggo::platform::nx::deko
