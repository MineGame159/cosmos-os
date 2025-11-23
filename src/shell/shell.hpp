#pragma once

#include "color.hpp"

#include <cstdarg>

namespace cosmos::shell {
    [[noreturn]]
    void run();

    void set_color(Color color);

    void print(const char* str);
    void printf(const char* fmt, va_list args);

    void read(char* buffer, uint32_t length);

    // Get the shell's current working directory (owned by shell, do not free)
    const char* get_cwd();

    // Set the shell's current working directory. Expects an absolute path.
    // Returns true on success, false on invalid path.
    bool set_cwd(const char* absolute_path);

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
