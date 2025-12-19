#pragma once

#include "vfs/types.hpp"

#include <cstdint>

namespace cosmos::devices::pit {
    constexpr uint64_t IOCTL_CREATE_EVENT = 1;

    using HandlerFn = void (*)(uint64_t);

    void init(vfs::Node* node);

    bool run_every_x_ms(uint64_t ms, HandlerFn fn, uint64_t data);
} // namespace cosmos::devices::pit
