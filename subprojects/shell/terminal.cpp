#include "terminal.hpp"

#include "font.hpp"
#include "keyboard.hpp"
#include "nanoprintf.h"
#include "syscalls.hpp"

namespace terminal {
    // Framebuffer
    static uint32_t fb;

    static uint32_t width;
    static uint32_t height;
    static uint32_t pitch;

    static uint32_t columns;
    static uint32_t rows;

    // Cursor
    static uint32_t row = 0;
    static uint32_t column = 0;

    static bool cursor_visible = false;
    static uint32_t blink_event;

    // Keyboard
    static bool caps_lock = false;
    static bool shift = false;

    // Colors
    static Color fg_color = WHITE;

    void init() {
        // Framebuffer
        if (!sys::open("/dev/framebuffer", sys::Mode::ReadWrite, fb)) {
            sys::exit(1);
        }

        const auto info = sys::ioctl(fb, 1, 0);
        width = info & 0xFFFF;
        height = (info >> 16) & 0xFFFF;
        pitch = (info >> 32) & 0xFFFF;

        columns = width / FONT_WIDTH;
        rows = height / FONT_HEIGHT;

        // Cursor blink timer event
        uint32_t timer;
        if (!sys::open("/dev/timer", sys::Mode::Read, timer)) {
            sys::exit(1);
        }

        blink_event = sys::ioctl(timer, 1, 500);
        sys::close(timer);

        // Clear screen
        const auto line_size = pitch * sizeof(uint32_t);
        const auto line = static_cast<uint32_t*>(__builtin_alloca(line_size));

        for (auto x = 0u; x < width; x++) {
            line[x] = 0xFF000000;
        }

        for (auto y = 0u; y < height; y++) {
            sys::write(fb, line, line_size);
        }
    }

    static void print(const char ch) {
        const auto glyph = get_font_glyph(ch);
        if (!glyph.valid()) return;

        const auto base_x = row * FONT_WIDTH;
        const auto base_y = column * FONT_HEIGHT;

        const auto color = fg_color.pack();

        for (auto y = 0u; y < FONT_HEIGHT; y++) {
            uint32_t line_pixels[FONT_WIDTH];

            for (auto x = 0u; x < FONT_WIDTH; x++) {
                line_pixels[x] = glyph.is_set(x, y) ? color : 0xFF000000;
            }

            sys::seek(fb, sys::SeekType::Start, ((base_y + y) * pitch + base_x) * 4);
            sys::write(fb, line_pixels, FONT_WIDTH * 4);
        }
    }

    static void fill_cell(const uint32_t pixel) {
        const auto base_x = row * FONT_WIDTH;
        const auto base_y = column * FONT_HEIGHT;

        uint32_t line_pixels[FONT_WIDTH];

        for (auto x = 0u; x < FONT_WIDTH; x++) {
            line_pixels[x] = pixel;
        }

        for (auto y = 0u; y < FONT_HEIGHT; y++) {
            sys::seek(fb, sys::SeekType::Start, ((base_y + y) * pitch + base_x) * 4);
            sys::write(fb, line_pixels, FONT_WIDTH * 4);
        }
    }

    static void new_line() {
        column++;

        if (column >= rows) {
            const auto line_size = pitch * 4;
            const auto line_pixels = __builtin_alloca(line_size);

            for (auto y = FONT_HEIGHT; y < height; y++) {
                sys::seek(fb, sys::SeekType::Start, y * line_size);
                sys::read(fb, line_pixels, line_size);

                sys::seek(fb, sys::SeekType::Start, (y - 16) * line_size);
                sys::write(fb, line_pixels, line_size);
            }

            column--;

            for (auto x = 0u; x < columns; x++) {
                row = x;
                fill_cell(0xFF000000);
            }
        }

        row = 0;
    }

    void set_fg_color(const Color color) {
        fg_color = color;
    }

    Color get_fg_color() {
        return fg_color;
    }

    void print(const stl::StringView str) {
        for (const auto ch : str) {
            if (ch == '\n') {
                new_line();
            } else if (ch != '\0') {
                print(ch);

                row++;
                if (row >= columns) new_line();
            }
        }
    }

    void printf_args(const char* fmt, va_list args) {
        static char buffer[256];

        const auto size = npf_vsnprintf(buffer, 256, fmt, args);
        print(stl::StringView(buffer, size));
    }

    static bool get_char_from_event(const KeyEvent event, char& ch) {
        if (event.key == Key::CapsLock) {
            if ((!caps_lock && event.press) || (caps_lock && !event.press)) caps_lock = !caps_lock;
            return false;
        }

        if (event.key == Key::LeftShift || event.key == Key::RightShift) {
            shift = event.press;
            return false;
        }

        if (!event.press) {
            return false;
        }

        if (event.key >= Key::A && event.key <= Key::Z) {
            if (caps_lock || shift)
                ch = static_cast<char>('A' + (static_cast<uint8_t>(event.key) - static_cast<uint8_t>(Key::A)));
            else
                ch = static_cast<char>('a' + (static_cast<uint8_t>(event.key) - static_cast<uint8_t>(Key::A)));
            return true;
        }

        if (event.key >= Key::Key0 && event.key <= Key::Key9) {
            ch = static_cast<char>('0' + (static_cast<uint8_t>(event.key) - static_cast<uint8_t>(Key::Key0)));
            return true;
        }

        if (event.key >= Key::Num0 && event.key <= Key::Num9) {
            ch = static_cast<char>('0' + (static_cast<uint8_t>(event.key) - static_cast<uint8_t>(Key::Num0)));
            return true;
        }

        switch (event.key) {
        case Key::Space:
            ch = ' ';
            return true;
        case Key::NumSlash:
        case Key::Slash:
            ch = '/';
            return true;
        case Key::NumPeriod:
        case Key::Period:
            ch = '.';
            return true;
        case Key::Comma:
            ch = ',';
            return true;
        default:
            return false;
        }
    }

    uint64_t read(char* buffer, const uint64_t length) {
        uint32_t kb;
        if (!sys::open("/dev/keyboard", sys::Mode::Read, kb)) return 0;

        const uint32_t kb_event = sys::ioctl(kb, 1, 0); // create keyboard event
        sys::ioctl(kb, 2, 0);                           // reset keyboard buffer

        uint64_t size = 0;

        for (;;) {
            fill_cell(cursor_visible ? 0xFFFFFFFF : 0xFF000000);

            const uint32_t events[] = { blink_event, kb_event };
            uint64_t signalled;
            sys::poll(events, 2, true, signalled);

            if (signalled & 0b01) {
                cursor_visible = !cursor_visible;
            }

            if (signalled & 0b10) {
                KeyEvent event;
                uint64_t read;

                auto exit = false;

                while (sys::read(kb, &event, sizeof(KeyEvent), read) && read != 0) {
                    if ((event.key == Key::Enter || event.key == Key::NumEnter) && event.press) {
                        if (size > 0) {
                            exit = true;
                            break;
                        }
                    }

                    if (event.key == Key::Backspace && event.press) {
                        if (size > 0) {
                            if (cursor_visible) fill_cell(0xFF000000);

                            size--;
                            row--;

                            fill_cell(0xFF000000);
                        }

                        continue;
                    }

                    char ch;
                    if (get_char_from_event(event, ch) && size < length - 1) {
                        buffer[size++] = ch;

                        if (cursor_visible) fill_cell(0xFF000000);

                        print(stl::StringView(&ch, 1));
                    }
                }

                if (exit) break;
            }
        }

        sys::close(kb_event);
        sys::close(kb);

        if (cursor_visible) fill_cell(0xFF000000);

        print(stl::StringView("\n", 1));
        buffer[size] = '\0';

        return size;
    }
} // namespace terminal
