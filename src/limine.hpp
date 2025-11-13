#pragma once

#include <cstdint>

namespace cosmos::limine {
    struct Framebuffer {
        uint32_t width;
        uint32_t height;
        uint32_t pitch;
        void* pixels;
    };

    bool init();

    const Framebuffer& get_framebuffer();
} // namespace cosmos::limine
