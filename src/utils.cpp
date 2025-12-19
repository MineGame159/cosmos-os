#include "utils.hpp"

#include "log/display.hpp"
#include "log/log.hpp"
#include "memory/heap.hpp"
#include "nanoprintf.h"

#include <cstdarg>

namespace cosmos::utils {
    void panic_print_regs(const char* r0_name, const uint64_t r0, const char* r1_name, const uint64_t r1, const char* r2_name,
                          const uint64_t r2) {
        log::display::printf(shell::WHITE, "  %s=", r0_name);
        log::display::printf(shell::GRAY, "0x%016llX", r0);

        log::display::printf(shell::WHITE, " %s=", r1_name);
        log::display::printf(shell::GRAY, "0x%016llX", r1);

        log::display::printf(shell::WHITE, " %s=", r2_name);
        log::display::printf(shell::GRAY, "0x%016llX\n", r2);
    }

    struct Frame {
        Frame* previous;
        uint64_t return_address;
    };

    bool is_address_safe(const uint64_t addr) {
        // Basic check: must be non-null and likely in higher half for kernel
        if (addr == 0) return false;
        // Check alignment (stack frames are 8-byte aligned)
        if (addr & 0x7) return false;

        // Check if canonical (simplified for example)
        return (addr <= 0x00007FFFFFFFFFFF) || (addr >= 0xFFFF800000000000);
    }

    void panic_print_stack_frame(const uint64_t index, const uint64_t address) {
        log::display::printf(shell::WHITE, "  Frame ");
        log::display::printf(shell::GRAY, "%d", index);
        log::display::printf(shell::WHITE, ": ");
        log::display::printf(shell::GRAY, "0x%016llX\n", address);
    }

    void panic_print_stack_trace(uint64_t rbp) {
        auto frame = reinterpret_cast<Frame*>(rbp);
        if (frame == nullptr) asm volatile("mov %%rbp, %0" : "=r"(frame));

        const auto offset = rbp == 0 ? 0 : 1;

        for (auto i = 0; frame != nullptr && i < 32; i++) {
            if (!is_address_safe(reinterpret_cast<uint64_t>(frame))) {
                break;
            }

            if (frame->return_address != 0) {
                panic_print_stack_frame(i + offset, frame->return_address - 1);
            }

            frame = frame->previous;
        }
    }

    void panic(const isr::InterruptInfo* info, const char* fmt, ...) {
        asm volatile("cli" ::: "memory");

        va_list args;
        va_start(args, fmt);
        char buffer[256];
        npf_vsnprintf(buffer, 256 - 1, fmt, args);
        va_end(args);

        log::enable_display(false);
        log::display::printf(shell::WHITE, "\n");
        log::display::printf(shell::WHITE, " --- KERNEL PANIC ---\n");
        log::display::printf(shell::WHITE, "  %s", buffer);

        if (info != nullptr) {
            log::display::printf(shell::GRAY, " (%d", info->interrupt);
            log::display::printf(shell::WHITE, ") - ");
            log::display::printf(shell::GRAY, "%d", info->error);
        }

        log::display::printf(shell::WHITE, "\n\n");

        if (info != nullptr) {
            log::display::printf(shell::WHITE, " --- REGISTERS ---\n");
            panic_print_regs("RAX", info->rax, "RBX", info->rbx, "RCX", info->rcx);
            panic_print_regs("RDX", info->rdx, "RSI", info->rsi, "RDI", info->rdi);
            panic_print_regs("R8 ", info->r8, "R9 ", info->r9, "R10", info->r10);
            panic_print_regs("R11", info->r11, "R12", info->r12, "R13", info->r13);
            panic_print_regs("R14", info->r14, "R15", info->r15, "RBP", info->rbp);
            log::display::printf(shell::WHITE, "\n");

            log::display::printf(shell::WHITE, " --- STACK TRACE ---\n");
            panic_print_stack_frame(0, info->iret_rip);
        } else {
            log::display::printf(shell::WHITE, " --- STACK TRACE ---\n");
        }

        panic_print_stack_trace(info != nullptr ? info->rbp : 0);
        log::display::printf(shell::WHITE, "\n");

        halt();
    }

    void halt() {
        WARN("System halted");

        asm volatile("cli" ::: "memory");

        for (;;) {
            asm volatile("hlt" ::: "memory");
        }
    }

    void cpuid(uint32_t arg, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) { // NOLINT(*-non-const-parameter)
        asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(arg));
    }

    void memset(void* dst, const uint8_t value, const std::size_t size) {
        for (uint64_t i = 0; i < size; i++) {
            static_cast<uint8_t*>(dst)[i] = value;
        }
    }

    void memcpy(void* dst, const void* src, std::size_t size) {
        // Copy 1 byte at a time
        if (size < 128) {
            for (uint64_t i = 0; i < size; i++) {
                static_cast<uint8_t*>(dst)[i] = static_cast<const uint8_t*>(src)[i];
            }

            return;
        }

        // Copy 8 bytes at a time
        const auto size64 = size / 8;

        for (uint64_t i = 0; i < size64; i++) {
            static_cast<uint64_t*>(dst)[i] = static_cast<const uint64_t*>(src)[i];
        }

        dst = static_cast<uint8_t*>(dst) + size64 * 8;
        src = static_cast<const uint8_t*>(src) + size64 * 8;
        size -= size64 * 8;

        for (uint64_t i = 0; i < size; i++) {
            static_cast<uint8_t*>(dst)[i] = static_cast<const uint8_t*>(src)[i];
        }
    }

    uint8_t memcmp(const void* lhs, const void* rhs, const std::size_t size) {
        const auto lhs_ = static_cast<const uint8_t*>(lhs);
        const auto rhs_ = static_cast<const uint8_t*>(rhs);

        for (std::size_t i = 0; i < size; i++) {
            if (lhs_[i] < rhs_[i]) return -1;
            if (lhs_[i] > rhs_[i]) return 1;
        }

        return 0;
    }

    uint32_t strlen(const char* str) {
        auto length = 0u;

        while (*str != '\0') {
            length++;
            str++;
        }

        return length;
    }

    char* strdup(const char* str, const uint32_t str_length) {
        const auto dup = memory::heap::alloc_array<char>(str_length + 1);

        memcpy(dup, str, str_length);
        dup[str_length] = '\0';

        return dup;
    }

    bool streq(const char* a, const char* b) {
        while (*a == *b) {
            if (*a == '\0') return true;

            a++;
            b++;
        }

        return false;
    }

    bool streq(const char* a, uint32_t const a_length, const char* b, const uint32_t b_length) {
        if (a_length != b_length) return false;

        for (auto i = 0u; i < a_length; i++) {
            if (a[i] != b[i]) return false;
        }

        return true;
    }

    bool str_has_prefix(const char* str, const char* prefix) {
        while (*prefix != '\0') {
            if (*str != *prefix) return false;

            str++;
            prefix++;
        }

        return true;
    }

    int32_t str_index_of(const char* str, const char ch) {
        auto i = 0;

        while (str[i] != '\0') {
            if (str[i] == ch) return i;
            i++;
        }

        return -1;
    }

    const char* str_trim_left(const char* str) {
        while (*str == ' ') {
            str++;
        }

        return str;
    }
} // namespace cosmos::utils

extern "C" {
// mem...

void* memset(void* dest, const int ch, const std::size_t count) {
    cosmos::utils::memset(dest, ch, count);
    return dest;
}

void* memcpy(void* dest, const void* src, const std::size_t count) {
    cosmos::utils::memcpy(dest, src, count);
    return dest;
}


int memcmp(const void* lhs, const void* rhs, const std::size_t size) {
    return cosmos::utils::memcmp(lhs, rhs, size);
}

// alloc, free

void* aligned_alloc(const size_t size, const size_t alignment) {
    return cosmos::memory::heap::alloc(size, alignment);
}

void free(void* ptr) {
    cosmos::memory::heap::free(ptr);
}
}
