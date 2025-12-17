#pragma once

#include <cstdint>

namespace cosmos::memory::virt {
    void init_range_alloc();

    uint64_t alloc_range(uint64_t page_count);
    void free_range(uint64_t first_page);
} // namespace cosmos::memory::virt
