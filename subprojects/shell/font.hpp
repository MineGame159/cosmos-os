#pragma once

#include <cstdint>

constexpr uint32_t FONT_WIDTH = 8;
constexpr uint32_t FONT_HEIGHT = 16;
constexpr uint32_t FONT_SIZE = FONT_WIDTH * FONT_HEIGHT;

struct Glyph {
    const uint8_t* ptr;

    [[nodiscard]]
    bool valid() const {
        return ptr != nullptr;
    }

    [[nodiscard]]
    bool is_set(const uint32_t x, const uint32_t y) const {
        const auto offset = y < 8 ? 0 : 8;
        const auto mask = 1 << (y - offset);

        return (ptr[x + offset] & mask) == mask;
    }
};

Glyph get_font_glyph(char ch);
