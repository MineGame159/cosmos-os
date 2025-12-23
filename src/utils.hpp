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

    // MSR

    constexpr uint32_t MSR_EFER = 0xC0000080;
    constexpr uint32_t MSR_STAR = 0xC0000081;
    constexpr uint32_t MSR_LSTAR = 0xC0000082;
    constexpr uint32_t MSR_SFMASK = 0xC0000084;
    constexpr uint32_t MSR_KERNEL_GS_BASE = 0xC0000102;
    constexpr uint32_t MSR_GS_BASE = 0xC0000101;

    inline uint64_t msr_read(const uint32_t msr) {
        uint32_t lo;
        uint32_t hi;
        asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr) : "memory");
        return (static_cast<uint64_t>(hi) << 32) | lo;
    }

    inline void msr_write(const uint32_t msr, const uint64_t value) {
        const auto lo = value & 0xFFFFFFFF;
        const auto hi = (value >> 32) & 0xFFFFFFFF;
        asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr) : "memory");
    }

    // Byte

    inline uint8_t byte_in(uint16_t port) {
        uint8_t result;
        asm volatile("in %%dx, %%al" : "=a"(result) : "d"(port));
        return result;
    }

    inline void byte_out(uint16_t port, uint8_t data) {
        asm volatile("out %%al, %%dx" : : "a"(data), "d"(port));
    }

    // Short

    inline uint16_t short_in(uint16_t port) {
        uint16_t result;
        asm volatile("in %%dx, %%ax" : "=a"(result) : "d"(port));
        return result;
    }

    inline void short_out(uint16_t port, uint16_t data) {
        asm volatile("out %%ax, %%dx" : : "a"(data), "d"(port));
    }

    // Int

    inline uint32_t int_in(uint16_t port) {
        uint32_t result;
        asm volatile("in %%dx, %%eax" : "=a"(result) : "d"(port));
        return result;
    }

    inline void int_out(uint16_t port, uint32_t data) {
        asm volatile("out %%eax, %%dx" : : "a"(data), "d"(port));
    }

    // Other

    inline void wait() {
        byte_out(0x80, 0);
    }
} // namespace cosmos::utils
