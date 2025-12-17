#pragma once

#include "interrupts/info.hpp"

#include <cstdint>

namespace cosmos::utils {
    [[noreturn]]
    void panic(const isr::InterruptInfo* info, const char* fmt, ...);

    [[noreturn]]
    void halt();

    void cpuid(uint32_t arg, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);

    void memset(void* dst, uint8_t value, std::size_t size);
    void memcpy(void* dst, const void* src, std::size_t size);
    uint8_t memcmp(const void* lhs, const void* rhs, std::size_t size);

    uint32_t strlen(const char* str);
    char* strdup(const char* str, uint32_t str_length);

    bool streq(const char* a, const char* b);
    bool streq(const char* a, uint32_t a_length, const char* b, uint32_t b_length);

    bool str_has_prefix(const char* str, const char* prefix);
    int32_t str_index_of(const char* str, char ch);
    const char* str_trim_left(const char* str);

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
    T align_up(T value, T alignment) {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    template <typename T>
    T align_down(T value, T alignment) {
        return value & ~(alignment - 1);
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
