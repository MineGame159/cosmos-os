#pragma once

#include <cstdint>

namespace cosmos::memory::virt {
    constexpr uint64_t GB = 512ul * 512ul * 4096ul;

    constexpr uint64_t LOWER_HALF_END = 0x0000800000000000;

    /// Direct map starts immediately at the higher half split
    constexpr uint64_t DIRECT_MAP = 0xFFFF800000000000;

    /// Framebuffer starts 128 gB after direct map
    constexpr uint64_t FRAMEBUFFER = DIRECT_MAP + (128ul * GB);

    /// Log starts 1 gB after framebuffer
    constexpr uint64_t LOG = FRAMEBUFFER + (1ul * GB);

    /// Range allocator starts 1gB after framebuffer
    constexpr uint64_t RANGE_ALLOC = LOG + (1ul * GB);

    /// Heap starts 1 gB after range allocator
    constexpr uint64_t HEAP = RANGE_ALLOC + (1ul * GB);

    /// Kernel starts at the last 2 gB of the entire address space
    constexpr uint64_t KERNEL = 0xFFFFFFFF80000000;

    constexpr bool is_kernel(const uint64_t addr) {
        return addr >= DIRECT_MAP;
    }
} // namespace cosmos::memory::virt
