#pragma once

#include <span>

namespace doggo::memory
{
    class LinearAllocator final
    {
      public:
        explicit LinearAllocator( std::span<std::byte> memory ) noexcept;

        [[nodiscard]] void * allocate( std::size_t size, std::size_t alignment = alignof( std::max_align_t ) ) noexcept;
        void                 reset() noexcept;

        [[nodiscard]] std::size_t getCapacity() const noexcept;
        [[nodiscard]] std::size_t getUsed() const noexcept;
        [[nodiscard]] std::size_t getRemaining() const noexcept;
        [[nodiscard]] std::size_t getPeakUsage() const noexcept;

      private:
        std::span<std::byte> mMemory;

        std::size_t mOffset    = 0;
        std::size_t mPeakUsage = 0;
    };
} // namespace doggo::memory
