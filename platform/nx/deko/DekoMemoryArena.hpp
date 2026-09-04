#pragma once

#include <cstdint>
#include <optional>

#include <deko3d.hpp>

namespace doggo::platform::nx::deko
{
    struct DekoMemorySlice
    {
        std::uint32_t mOffset = 0;
        std::uint32_t mSize   = 0;
    };

    class DekoMemoryArena final
    {
      public:
        [[nodiscard]] bool initialize( DkDevice device, std::uint32_t capacity, std::uint32_t flags ) noexcept;

        [[nodiscard]] std::optional<DekoMemorySlice> allocate( std::uint32_t size, std::uint32_t alignment ) noexcept;

        [[nodiscard]] DkMemBlock    getMemoryBlock() const noexcept;
        [[nodiscard]] void *        getCpuAddress( const DekoMemorySlice & slice ) noexcept;
        [[nodiscard]] DkGpuAddr     getGpuAddress( const DekoMemorySlice & slice ) const noexcept;
        [[nodiscard]] std::uint32_t getCapacity() const noexcept;
        [[nodiscard]] std::uint32_t getUsed() const noexcept;
        [[nodiscard]] std::uint32_t getRemaining() const noexcept;

      private:
        dk::UniqueMemBlock mMemory;
        std::uint32_t      mCapacity = 0;
        std::uint32_t      mOffset   = 0;
    };
} // namespace doggo::platform::nx::deko
