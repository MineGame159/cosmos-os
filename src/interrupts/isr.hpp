#pragma once

#include "info.hpp"

namespace cosmos::isr {
    typedef void (*handler_fn)(InterruptInfo* info);

    void init();

    void set(uint8_t num, handler_fn handler);
} // namespace cosmos::isr
