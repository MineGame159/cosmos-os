#pragma once

#include "vfs/types.hpp"

namespace cosmos::devices::framebuffer {
    constexpr uint32_t IOCTL_GET_INFO = 1;

    void init(vfs::Node* node);
} // namespace cosmos::devices::framebuffer
