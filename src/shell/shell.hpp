#pragma once

#include "color.hpp"

#include <cstdarg>

namespace cosmos::shell {
    [[noreturn]]
    void run();

    void set_color(Color color);

    void print(const char* str, uint64_t len);
    void print(const char* str);
    void printf(const char* fmt, va_list args);

    void read(char* buffer, uint32_t length);

    inline void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        printf(fmt, args);

        va_end(args);
    }

    inline void print(const Color color, const char* str) {
        set_color(color);
        print(str);
        set_color(WHITE);
    }

    inline void printf(const Color color, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        set_color(color);
        printf(fmt, args);
        set_color(WHITE);

        va_end(args);
    }
} // namespace cosmos::shell
