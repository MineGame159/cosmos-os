#include "utils.hpp"

#include "serial.hpp"

namespace cosmos::utils {
    void halt() {
        serial::print("[cosmos] System halted\n");

        asm volatile("cli");

        for (;;) {
            asm volatile("hlt");
        }
    }

    void memset(void* dst, const uint8_t value, const uint64_t size) {
        for (uint64_t i = 0; i < size; i++) {
            static_cast<uint8_t*>(dst)[i] = value;
        }
    }

    void memcpy(void* dst, void* src, uint64_t size) {
        // Copy 1 byte at a time
        if (size < 128) {
            for (uint64_t i = 0; i < size; i++) {
                static_cast<uint8_t*>(dst)[i] = static_cast<uint8_t*>(src)[i];
            }

            return;
        }

        // Copy 8 bytes at a time
        const auto size64 = size / 8;

        for (uint64_t i = 0; i < size64; i++) {
            static_cast<uint64_t*>(dst)[i] = static_cast<uint64_t*>(src)[i];
        }

        dst = static_cast<uint8_t*>(dst) + size64 * 8;
        src = static_cast<uint8_t*>(src) + size64 * 8;
        size -= size64 * 8;

        for (uint64_t i = 0; i < size; i++) {
            static_cast<uint8_t*>(dst)[i] = static_cast<uint8_t*>(src)[i];
        }
    }
} // namespace cosmos::utils
