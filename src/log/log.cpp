#include "log.hpp"

#include "display.hpp"
#include "memory/offsets.hpp"
#include "memory/physical.hpp"
#include "memory/virtual.hpp"
#include "nanoprintf.h"
#include "serial.hpp"
#include "utils.hpp"

#include <cstdarg>

namespace cosmos::log {
    static bool display_enabled = false;
    static bool paging_enabled = false;

    [[gnu::aligned(4096)]]
    static uint8_t initial_page[4096];
    static uint8_t* start = initial_page;
    static uint64_t size = 0;
    static uint32_t capacity = 4096;

    void print(const shell::Color color, const char* str) {
        // Serial
        serial::print(str);

        // Display
        if (display_enabled) {
            display::print(color, str);
        }

        // Memory
        auto length = utils::strlen(str);
        if (length > 0 && str[length - 1] == '\n') length++;

        while (size + length > capacity) {
            if (!paging_enabled) return;

            const auto phys = memory::phys::alloc_pages(1);
            const auto space = memory::virt::get_current();

            if (!memory::virt::map_pages(space, (memory::virt::LOG + capacity) / 4096, phys / 4096, 1, memory::virt::Flags::Write)) {
                return;
            }

            capacity += 4096;
        }

        utils::memcpy(&start[size], str, length);
        size += length;
    }

    void print_type(const Type type) {
        switch (type) {
        case Type::Debug:
            print(shell::GRAY, "[DEBG] ");
            break;
        case Type::Info:
            print(shell::GREEN, "[INFO] ");
            break;
        case Type::Warning:
            print(shell::YELLOW, "[WARN] ");
            break;
        case Type::Error:
            print(shell::RED, "[ERR ] ");
            break;
        default:
            print(shell::BLUE, "[????] ");
            break;
        }
    }

    void print_file(const shell::Color color, const char* file) {
        if (utils::str_has_prefix(file, "../src/")) {
            file = &file[7];
        }

        print(color, file);
    }

    void print_num(const shell::Color color, uint32_t num) {
        static char buffer[16];
        auto len = 0u;

        do {
            buffer[len++] = static_cast<char>('0' + num % 10);
            num /= 10;
        } while (num != 0);

        for (auto i = 0u; i < len / 2; i++) {
            const auto temp = buffer[i];
            buffer[i] = buffer[len - i - 1];
            buffer[len - i - 1] = temp;
        }

        buffer[len] = '\0';
        print(color, buffer);
    }

    void enable_display(const bool delay) {
        if (!display_enabled) {
            display::init(delay);
        }

        display_enabled = true;
    }

    void disable_display() {
        display_enabled = false;
    }

    void enable_paging() {
        if (paging_enabled) return;

        const auto phys = memory::virt::get_phys(reinterpret_cast<uint64_t>(initial_page));
        const auto space = memory::virt::get_current();

        if (!memory::virt::map_pages(space, memory::virt::LOG / 4096, phys / 4096, 1, memory::virt::Flags::Write)) {
            return;
        }

        paging_enabled = true;
        start = reinterpret_cast<uint8_t*>(memory::virt::LOG);
    }

    void println_args(const Type type, const char* file, const uint32_t line, const char* fmt, va_list args) {
        static char buffer[256];

        print_type(type);
        print_file(shell::WHITE, file);

        print(shell::GRAY, ":");
        print_num(shell::GRAY, line);
        print(shell::GRAY, " - ");

        auto len = npf_vsnprintf(buffer, 256, fmt, args);

        while (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }

        print(shell::WHITE, buffer);
        print(shell::WHITE, "\n");
    }

    void println(const Type type, const char* file, const uint32_t line, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        println_args(type, file, line, fmt, args);
        va_end(args);
    }

    const uint8_t* get_start() {
        return start;
    }

    uint64_t get_size() {
        return size;
    }
} // namespace cosmos::log
