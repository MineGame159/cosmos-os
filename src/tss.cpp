#include "tss.hpp"

#include <cstdint>

namespace cosmos::tss {
    struct [[gnu::packed]] Tss {
        uint32_t reserved0;
        uint64_t rsp[3];
        uint64_t reserved1;
        uint64_t ist[7];
        uint64_t reserved2;
        uint16_t reserved3;
        uint16_t iomap_base;
    };

    static volatile Tss tss = {};

    void init() {
        tss.iomap_base = sizeof(Tss);

        asm volatile("ltr %0" : : "r"(static_cast<uint16_t>(0x28)) : "memory");
    }

    void set_rsp(const uint8_t level, const uint64_t rsp) {
        if (level < 3) {
            tss.rsp[level] = rsp;
        }
    }

    uint64_t get_address() {
        return reinterpret_cast<uint64_t>(&tss);
    }

    uint64_t get_size() {
        return sizeof(Tss);
    }
} // namespace cosmos::tss
