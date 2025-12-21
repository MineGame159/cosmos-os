#pragma once

#include "color.hpp"

namespace cosmos::log::display {
    void init(bool delay = false);

    void print(Color color, const char* str);

    void printf(Color color, const char* fmt, ...);
} // namespace cosmos::log::display
