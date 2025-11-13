#pragma once

namespace cosmos::serial {
    bool init();

    void print(const char* str);

    void printf(const char* fmt, ...);
} // namespace cosmos::serial
