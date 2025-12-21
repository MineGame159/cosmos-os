#pragma once

#include "color.hpp"
#include "stl/string_view.hpp"

#include <cstdarg>

namespace terminal {
    void init();

    void set_fg_color(Color color);
    Color get_fg_color();

    void print(stl::StringView str);
    void printf_args(const char* fmt, va_list args);

    uint64_t read(char* buffer, uint64_t length);

    inline void print(const Color color, const stl::StringView str) {
        const auto prev = get_fg_color();
        set_fg_color(color);
        print(str);
        set_fg_color(prev);
    }

    inline void printf(const Color color, const char* fmt, va_list args) {
        const auto prev = get_fg_color();
        set_fg_color(color);
        printf_args(fmt, args);
        set_fg_color(prev);
    }

    inline void printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printf_args(fmt, args);
        va_end(args);
    }

    inline void printf(const Color color, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printf(color, fmt, args);
        va_end(args);
    }
} // namespace terminal
