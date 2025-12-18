#include "gdt.hpp"

#include "log/log.hpp"
#include "tss.hpp"

#include <cstdint>

namespace cosmos::gdt {
    constexpr uint8_t FLAGS_LONG = 0b001'0;
    constexpr uint8_t FLAGS_SIZE = 0b010'0;
    constexpr uint8_t FLAGS_PAGE = 0b100'0;

    constexpr uint8_t ACCESS_ACCESSED /**/ = 0b00000001;
    constexpr uint8_t ACCESS_RW /*      */ = 0b00000010;
    constexpr uint8_t ACCESS_DC /*      */ = 0b00000100;
    constexpr uint8_t ACCESS_EXEC /*    */ = 0b00001000;
    constexpr uint8_t ACCESS_NOTSYS /*  */ = 0b00010000;
    constexpr uint8_t ACCESS_USER /*    */ = 0b01100000;
    constexpr uint8_t ACCESS_PRESENT /* */ = 0b10000000;

    struct [[gnu::packed]] Entry {
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t base_mid;
        uint8_t access;
        uint8_t limit_high_flags;
        uint8_t base_high;
    };

    struct [[gnu::packed]] Descriptor {
        uint16_t size;
        void* address;
    };

    static Entry entries[7];
    static Descriptor descriptor;

    Entry entry(const uint32_t base, const uint32_t limit, const uint8_t access, const uint8_t flags) {
        return {
            .limit_low = static_cast<uint16_t>(limit & 0xFFFF),
            .base_low = static_cast<uint16_t>(base & 0xFFFF),
            .base_mid = static_cast<uint8_t>((base >> 16) & 0xFF),
            .access = access,
            .limit_high_flags = static_cast<uint8_t>(((limit >> 16) & 0xF) | ((flags & 0xF) << 4)),
            .base_high = static_cast<uint8_t>((base >> 24) & 0xFF),
        };
    }

    void init() {
        constexpr auto base_access = ACCESS_PRESENT | ACCESS_RW | ACCESS_NOTSYS | ACCESS_ACCESSED;

        entries[0] = entry(0, 0, 0, 0);                                                // Null
        entries[1] = entry(0, 0, base_access | ACCESS_EXEC, FLAGS_LONG);               // Kernel - Code
        entries[2] = entry(0, 0, base_access, 0);                                      // Kernel - Data
        entries[3] = entry(0, 0, base_access | ACCESS_USER, 0);                        // User - Data
        entries[4] = entry(0, 0, base_access | ACCESS_EXEC | ACCESS_USER, FLAGS_LONG); // User - Code

        // TSS
        const uint64_t tss_base = tss::get_address();
        const uint32_t tss_limit = tss::get_size();
        constexpr auto access = ACCESS_PRESENT | ACCESS_EXEC | ACCESS_ACCESSED;

        entries[5] = entry(static_cast<uint32_t>(tss_base), tss_limit, access, 0);

        entries[6] = {
            .limit_low = static_cast<uint16_t>((tss_base >> 32) & 0xFFFF),
            .base_low = static_cast<uint16_t>((tss_base >> 48) & 0xFFFF),
            .base_mid = 0,
            .access = 0,
            .limit_high_flags = 0,
            .base_high = 0,
        };

        // Load GDT
        descriptor = {
            .size = sizeof(entries) - 1,
            .address = entries,
        };

        asm volatile("lgdt %0" : : "m"(descriptor));

        asm volatile(R"(
            pushq $8
            leaq 1f(%%rip), %%rax
            pushq %%rax
            lretq

        1:
            movw $16, %%ax
            movw %%ax, %%ds
            movw %%ax, %%es
            movw %%ax, %%fs
            movw %%ax, %%gs
            movw %%ax, %%ss
        )" ::
                         : "memory");

        INFO("Switched GDT");
    }
} // namespace cosmos::gdt
