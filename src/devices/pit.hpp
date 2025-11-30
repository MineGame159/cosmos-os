#pragma once

#include "scheduler/event.hpp"


#include <cstdint>

namespace cosmos::devices::pit {
    using HandlerFn = void (*)(uint64_t);

    void start();

    bool run_every_x_ms(uint64_t ms, HandlerFn fn, uint64_t data);

    scheduler::EventHandle create_timer(uint64_t ms);
} // namespace cosmos::devices::pit
