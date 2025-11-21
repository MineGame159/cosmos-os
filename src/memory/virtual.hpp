#pragma once

#include <cstdint>

namespace cosmos::memory::virt {
    // Address

    struct Address {
        uint16_t pml4;
        uint16_t pdp;
        uint16_t pd;
        uint16_t pt;
        uint16_t offset;
    };

    Address unpack(uint64_t virt);
    uint64_t pack(Address addr);

    // Space

    using Space = uint64_t;

    Space create();
    Space get_current();

    /// NOTE: This function frees not only the memory used for the paging tables BUT ALSO the memory pointed to by the paging table entries,
    /// meaning it assumes full ownership of the underlying memory
    void destroy(Space space);

    bool map_pages(Space space, uint64_t virt, uint64_t phys, uint64_t count, bool cache_disabled);

    void switch_to(Space space);

    uint64_t get_phys(uint64_t virt);
} // namespace cosmos::memory::virt
