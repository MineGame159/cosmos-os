#include "display.hpp"

#include "font.hpp"
#include "limine.hpp"
#include "memory/offsets.hpp"
#include "memory/virtual.hpp"
#include "nanoprintf.h"
#include "utils.hpp"

#include <cstdarg>
#include <cstdint>

namespace cosmos::log::display {
    static uint32_t width;
    static uint32_t height;
    static uint32_t pitch;

    static bool do_delay;
    static uint32_t row;
    static uint32_t column;

    uint32_t* get_pixels() {
        if (memory::virt::switched()) return reinterpret_cast<uint32_t*>(memory::virt::FRAMEBUFFER);
        return static_cast<uint32_t*>(limine::get_framebuffer().pixels);
    }

    void init(const bool delay) {
        width = limine::get_framebuffer().width;
        height = limine::get_framebuffer().height;
        pitch = limine::get_framebuffer().pitch;

        const auto pixels = get_pixels();

        for (auto y = 0u; y < height; y++) {
            for (auto x = 0u; x < width; x++) {
                pixels[y * pitch + x] = 0xFF000000;
            }
        }

        do_delay = delay;
        row = 0;
        column = 0;
    }

    void new_line() {
        row = 0;
        column++;

        if (column >= height / FONT_HEIGHT) {
            const auto row_size = FONT_HEIGHT * pitch;
            const auto pixels = get_pixels();

            utils::memcpy(&pixels[0], &pixels[row_size], (height * pitch - row_size) * 4);

            column--;

            for (auto i = 0u; i < row_size; i++) {
                pixels[(height * pitch - row_size) + i] = 0xFF000000;
            }
        }
    }

    void print(const Color color, const char ch) {
        if (ch == '\n') {
            new_line();
            return;
        }

        const auto glyph = get_font_glyph(ch);

        if (glyph.valid()) {
            const auto pixels = get_pixels();
            const auto pixel = color.pack();

            for (auto ch_y = 0u; ch_y < FONT_HEIGHT; ch_y++) {
                for (auto ch_x = 0u; ch_x < FONT_WIDTH; ch_x++) {
                    if (glyph.is_set(ch_x, ch_y)) {
                        pixels[(column * FONT_HEIGHT + ch_y) * pitch + (row * FONT_WIDTH + ch_x)] = pixel;
                    }
                }
            }
        }

        if (row >= width / FONT_WIDTH) {
            new_line();
        } else {
            row++;
        }
    }

    // ReSharper disable once CppParameterMayBeConst
    void print(const Color color, const char* str) {
        while (*str != '\0') {
            print(color, *str);
            str++;
        }

        if (do_delay) {
            for (auto i = 0; i < 1024 * 256; i++) {
                utils::wait();
            }
        }
    }

    void printf(const Color color, const char* fmt, ...) {
        static char buffer[256];

        va_list args;
        va_start(args, fmt);
        npf_vsnprintf(buffer, 256, fmt, args);
        va_end(args);

        print(color, buffer);
    }
} // namespace cosmos::log::display
