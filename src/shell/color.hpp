#pragma once

#include <cstdint>

namespace cosmos::shell {
    struct Color {
        uint8_t r;
        uint8_t g;
        uint8_t b;

        [[nodiscard]]
        uint32_t pack() const {
            return 0xFF000000 | (r << 16) | (g << 8) | (b << 0);
        }
    };

    constexpr Color WHITE = { 255, 255, 255 };
    constexpr Color GRAY = { 150, 150, 150 };
    constexpr Color RED = { 150, 0, 0 };
    constexpr Color GREEN = { 0, 150, 0 };
    constexpr Color BLUE = { 0, 0, 150 };
    constexpr Color CYAN = { 0, 180, 180 };
    constexpr Color DARK_CYAN = { 0, 100, 100 };
} // namespace cosmos::shell
