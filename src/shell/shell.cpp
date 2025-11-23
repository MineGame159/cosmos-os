#include "shell.hpp"

#include "commands.hpp"
#include "devices/pit.hpp"
#include "devices/ps2kbd.hpp"
#include "font.hpp"
#include "limine.hpp"
#include "memory/offsets.hpp"
#include "nanoprintf.h"
#include "scheduler/scheduler.hpp"
#include "utils.hpp"

#include "memory/heap.hpp"
#include "vfs/path.hpp"

#include <cstdint>

namespace cosmos::shell {
    static uint32_t* pixels;
    static uint32_t width;
    static uint32_t height;

    static uint32_t cursor_x = 0;
    static uint32_t cursor_y = 0;
    static bool cursor_visible = false;

    static Color fg_color;

    static scheduler::ProcessId process;

    static bool caps_lock = false;
    static bool shift = false;

    // Shell-owned CWD string. Always an absolute path.
    // If heap_allocated is true then cwd points to heap memory that must be freed when replaced.
    // Otherwise, it points to static "/".
    static char* cwd = nullptr;
    static bool cwd_heap_allocated = false;

    const char* get_cwd() {
        if (cwd == nullptr) return "/";
        return cwd;
    }

    bool set_cwd(const char* absolute_path) {
        if (absolute_path == nullptr) return false;

        const auto len = vfs::check_abs_path(absolute_path);
        if (len == 0) return false;

        auto* copy = static_cast<char*>(memory::heap::alloc(len + 1));
        utils::memcpy(copy, absolute_path, len);
        copy[len] = '\0';

        if (cwd_heap_allocated && cwd != nullptr) {
            memory::heap::free(cwd);
        }

        cwd = copy;
        cwd_heap_allocated = true;
        return true;
    }

    // Print the interactive prompt with a colored, bracketed and shortened cwd.
    static void print_prompt() {
        constexpr auto MAX_PROMPT_LEN = 32u;
        char buf[64];

        const auto cwd_str = get_cwd();
        const auto cwd_len = utils::strlen(cwd_str);

        if (cwd_len <= MAX_PROMPT_LEN) {
            utils::memcpy(buf, cwd_str, cwd_len);
            buf[cwd_len] = '\0';

            print(DARK_CYAN, "[");
            print(CYAN, buf);
            print(DARK_CYAN, "]");
            print(WHITE, " > ");
            return;
        }

        // shorten with ellipsis, keep trailing segments up to MAX_PROMPT_LEN
        uint32_t take = 0;
        for (auto i = cwd_len; i > 0 && take < MAX_PROMPT_LEN - 5; ) {
            auto j = i;
            while (j > 0 && cwd_str[j - 1] != '/') j--;
            const auto seg_len = i - j;

            if (take + seg_len + 1 > MAX_PROMPT_LEN - 5) break;

            take += seg_len + 1; // include the '/'
            i = (j > 0) ? j - 1 : 0;
            if (i == 0) break;
        }

        if (take == 0) {
            take = MAX_PROMPT_LEN - 5;
        }

        const auto start = (cwd_len > take) ? (cwd_len - take) : 0;

        // build inner as "/...<trailing>"
        buf[0] = '/';
        buf[1] = '.';
        buf[2] = '.';
        buf[3] = '.';

        const auto copy_len = cwd_len - start;
        if (copy_len + 4 >= sizeof(buf) - 1) {
            const auto allowed = sizeof(buf) - 1 - 4;
            utils::memcpy(&buf[4], &cwd_str[cwd_len - allowed], allowed);
            buf[4 + allowed] = '\0';
        } else {
            utils::memcpy(&buf[4], &cwd_str[start], copy_len);
            buf[4 + copy_len] = '\0';
        }

        print(DARK_CYAN, "[");
        print(CYAN, buf);
        print(DARK_CYAN, "]");
        print(WHITE, " > ");
    }

    void blink_cursor([[maybe_unused]] uint64_t data) {
        cursor_visible = !cursor_visible;
        scheduler::resume(process);
    }

    void run() {
        const auto fb = limine::get_framebuffer();

        pixels = reinterpret_cast<uint32_t*>(memory::virt::FRAMEBUFFER);
        width = fb.width / FONT_WIDTH;
        height = fb.height / FONT_HEIGHT;

        fg_color = WHITE;

        process = scheduler::get_current_process();

        // initialize cwd to root
        cwd = const_cast<char*>("/");
        cwd_heap_allocated = false;

        devices::pit::run_every_x_ms(500, blink_cursor, 0);

        for (;;) {
            print_prompt();

            char prompt[128];
            read(prompt, 128);

            const auto space = utils::str_index_of(prompt, ' ');
            const auto name_length = space >= 0 ? space : utils::strlen(prompt);

            const auto cmd_fn = get_command_fn(prompt, name_length);

            if (cmd_fn == nullptr) {
                set_color(RED);
                print("Unknown command\n");
                set_color(WHITE);
                continue;
            }

            const auto args = utils::str_trim_left(&prompt[name_length]);
            cmd_fn(args);
        }
    }

    void print(const char ch) {
        const auto glyph = get_font_glyph(ch);

        const auto base_x = cursor_x * FONT_WIDTH;
        const auto base_y = cursor_y * FONT_HEIGHT;
        const auto line_size = width * FONT_WIDTH;

        const auto color = fg_color.pack();

        for (auto x = 0u; x < FONT_WIDTH; x++) {
            for (auto y = 0u; y < FONT_HEIGHT; y++) {
                if (glyph.is_set(x, y)) {
                    pixels[(base_y + y) * line_size + (base_x + x)] = color;
                }
            }
        }
    }

    void new_line() {
        cursor_x = 0;
        cursor_y++;

        if (cursor_y >= height) {
            const auto row_size = FONT_HEIGHT * width * FONT_WIDTH;

            utils::memcpy(pixels, &pixels[row_size], (height - 1) * row_size * 4);
            utils::memset(&pixels[(height - 1) * row_size], 0, row_size * 4);

            cursor_y--;
        }
    }

    void set_color(const Color color) {
        fg_color = color;
    }

    void print(const char* str) {
        char ch;

        while ((ch = *str) != '\0') {
            if (ch == '\n') {
                new_line();
            } else {
                print(ch);

                cursor_x++;
                if (cursor_x >= width) new_line();
            }

            str++;
        }
    }

    void printf(const char* fmt, va_list args) {
        static char buffer[256];
        npf_vsnprintf(buffer, 256, fmt, args);
        print(buffer);
    }

    void fill_cell(const uint32_t pixel) {
        const auto base_x = cursor_x * FONT_WIDTH;
        const auto base_y = cursor_y * FONT_HEIGHT;
        const auto line_size = width * FONT_WIDTH;

        for (auto x = 0u; x < FONT_WIDTH; x++) {
            for (auto y = 0u; y < FONT_HEIGHT; y++) {
                pixels[(base_y + y) * line_size + (base_x + x)] = pixel;
            }
        }
    }

    bool get_char_from_event(const devices::ps2kbd::Event event, char& ch) {
        using Key = devices::ps2kbd::Key;

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
        default:
            return false;
        }
    }

    void read(char* buffer, const uint32_t length) {
        devices::ps2kbd::reset_buffer();
        auto size = 0u;

        for (;;) {
            fill_cell(cursor_visible ? 0xFFFFFFFF : 0xFF000000);

            devices::ps2kbd::Event event;

            if (!devices::ps2kbd::try_get_event(event)) {
                devices::ps2kbd::resume_on_event();
                scheduler::suspend();
                continue;
            }

            if ((event.key == devices::ps2kbd::Key::Enter || event.key == devices::ps2kbd::Key::NumEnter) && event.press) {
                if (size > 0) break;
            }

            if (event.key == devices::ps2kbd::Key::Backspace && event.press) {
                if (size > 0) {
                    if (cursor_visible) fill_cell(0xFF000000);

                    size--;
                    cursor_x--;

                    fill_cell(0xFF000000);
                }

                continue;
            }

            char ch[2];
            if (get_char_from_event(event, ch[0]) && size < length - 1) {
                buffer[size++] = ch[0];

                if (cursor_visible) fill_cell(0xFF000000);

                ch[1] = '\0';
                print(ch);
            }
        }

        if (cursor_visible) fill_cell(0xFF000000);

        print("\n");
        buffer[size] = '\0';
    }
} // namespace cosmos::shell
