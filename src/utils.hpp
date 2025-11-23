#pragma once

#include <cstdint>

namespace cosmos::utils {
    [[noreturn]]
    void halt();

    void memset(void* dst, uint8_t value, uint64_t size);
    void memcpy(void* dst, const void* src, uint64_t size);

    uint32_t strlen(const char* str);

    bool streq(const char* a, const char* b);
    bool streq(const char* a, uint32_t a_length, const char* b, uint32_t b_length);

    bool str_has_prefix(const char* str, const char* prefix);

    template <typename T>
    T min(T a, T b) {
        return a < b ? a : b;
    }

    template <typename T>
    T max(T a, T b) {
        return a > b ? a : b;
    }

    template <typename T>
    T ceil_div(T a, T b) {
        return (a + b - 1) / b;
    }

    template <typename T>
    T align(T value, T alignment) {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    // Byte

    inline uint8_t byte_in(uint16_t port) {
        uint8_t result;
        asm volatile("in %%dx, %%al" : "=a"(result) : "d"(port));
        return result;
    }

    inline void byte_out(uint16_t port, uint8_t data) {
        asm volatile("out %%al, %%dx" ::"a"(data), "d"(port));
    }

    // Short

    inline uint16_t short_in(uint16_t port) {
        uint16_t result;
        asm volatile("in %%dx, %%ax" : "=a"(result) : "d"(port));
        return result;
    }

    inline void short_out(uint16_t port, uint16_t data) {
        asm volatile("out %%ax, %%dx" ::"a"(data), "d"(port));
    }

    // Int

    inline uint32_t int_in(uint16_t port) {
        uint32_t result;
        asm volatile("in %%dx, %%eax" : "=a"(result) : "d"(port));
        return result;
    }

    inline void int_out(uint16_t port, uint32_t data) {
        asm volatile("out %%eax, %%dx" ::"a"(data), "d"(port));
    }

    // Other

    inline void wait() {
        byte_out(0x80, 0);
    }
} // namespace cosmos::utils
