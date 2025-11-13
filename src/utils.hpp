#pragma once

#include <cstdint>

namespace cosmos::utils {
    [[noreturn]]
    void halt();

    // Byte

    inline uint8_t byte_in(uint16_t port) {
        uint8_t result;
        __asm__ volatile("in %%dx, %%al" : "=a"(result) : "d"(port));
        return result;
    }

    inline void byte_out(uint16_t port, uint8_t data) {
        __asm__ volatile("out %%al, %%dx" ::"a"(data), "d"(port));
    }

    // Short

    inline uint16_t short_in(uint16_t port) {
        uint16_t result;
        __asm__ volatile("in %%dx, %%ax" : "=a"(result) : "d"(port));
        return result;
    }

    inline void short_out(uint16_t port, uint16_t data) {
        __asm__ volatile("out %%ax, %%dx" ::"a"(data), "d"(port));
    }

    // Int

    inline uint32_t int_in(uint16_t port) {
        uint32_t result;
        __asm__ volatile("in %%dx, %%eax" : "=a"(result) : "d"(port));
        return result;
    }

    inline void int_out(uint16_t port, uint32_t data) {
        __asm__ volatile("out %%eax, %%dx" ::"a"(data), "d"(port));
    }

    // Other

    inline void wait() {
        byte_out(0x80, 0);
    }
} // namespace cosmos::utils
