#include "utils.hpp"

#include "memory/heap.hpp"
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

    void memcpy(void* dst, const void* src, uint64_t size) {
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

    uint32_t strlen(const char* str) {
        auto length = 0u;

        while (*str != '\0') {
            length++;
            str++;
        }

        return length;
    }

    char* strdup(const char* str, const uint32_t str_length) {
        const auto dup = static_cast<char*>(memory::heap::alloc(str_length + 1));

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
