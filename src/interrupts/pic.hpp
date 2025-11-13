#pragma once

#include <cstdint>

namespace cosmos::pic {
    void init();

    void set(uint8_t num, uint64_t handler, uint8_t flags);
    void update();

    void end_irq(uint8_t number);
} // namespace cosmos::pic
