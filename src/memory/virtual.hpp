#pragma once

#include "stl/bit_field.hpp"

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

    enum class Flags : uint8_t {
        None = 0,
        Write = 1 << 0,
        Execute = 1 << 1,
        Uncached = 1 << 2,
        User = 1 << 3,
    };
    ENUM_BIT_FIELD(Flags)

    using Space = uint64_t;

    Space create();
    Space get_current();

    /// Does a DEEP copy of the address space.
    /// Meaning that it not only copies the paging tables but also the physical pages pointed to by the paging tables.
    Space fork(Space other);

    /// NOTE: This function frees not only the memory used for the paging tables BUT ALSO the memory pointed to by the paging table entries,
    /// meaning it assumes full ownership of the underlying memory
    void destroy(Space space);

    bool map_pages(Space space, uint64_t virt, uint64_t phys, uint64_t count, Flags flags);

    void switch_to(Space space);
    bool switched();

    uint64_t get_phys(uint64_t virt);

    void dump(Space space, void (*range_fn)(uint64_t virt_start, uint64_t virt_end));

    inline void dump(void (*range_fn)(uint64_t virt_start, uint64_t virt_end)) {
        dump(get_current(), range_fn);
    }
} // namespace cosmos::memory::virt
