#pragma once

#include <cstdint>

namespace cosmos::memory::phys {
    void init();

    /**
     * @return physical address to the page or 0 if it failed to do so
     */
    uint64_t alloc_pages(uint32_t count);

    void free_pages(uint32_t first, uint32_t count);

    uint32_t get_total_pages();
    uint32_t get_used_pages();

    inline uint32_t get_free_pages() {
        return get_total_pages() - get_used_pages();
    }
} // namespace cosmos::memory::phys
