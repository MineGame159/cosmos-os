#pragma once

#include <cstdint>

namespace cosmos::devices::pit {
    using HandlerFn = void (*)(uint64_t);

    void start();

    void run_every_x_ms(uint64_t ms, HandlerFn fn, uint64_t data);
} // namespace cosmos::devices::pit
