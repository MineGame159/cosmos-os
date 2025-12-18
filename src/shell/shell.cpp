#include "shell.hpp"

#include "commands.hpp"
#include "devices/keyboard.hpp"
#include "devices/pit.hpp"
#include "font.hpp"
#include "limine.hpp"
#include "memory/heap.hpp"
#include "nanoprintf.h"
#include "scheduler/scheduler.hpp"
#include "utils.hpp"
#include "vfs/path.hpp"
#include "vfs/vfs.hpp"

#include <cstdint>

namespace cosmos::shell {
    static vfs::File* fbdev;
    static uint32_t pitch;
    static uint32_t columns;
    static uint32_t rows;

    static uint32_t row = 0;
    static uint32_t column = 0;
    static bool cursor_visible = false;

    static Color fg_color;

    static scheduler::EventHandle cursor_blink_event;

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

        auto* copy = memory::heap::alloc_array<char>(len + 1);
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
        for (auto i = cwd_len; i > 0 && take < MAX_PROMPT_LEN - 5;) {
            auto j = i;
            while (j > 0 && cwd_str[j - 1] != '/') {
                j--;
            }
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

    void fill_cell(uint32_t pixel);

    void run() {
        const auto fb = limine::get_framebuffer();

        fbdev = vfs::open_file("/dev/framebuffer", vfs::Mode::ReadWrite);
        pitch = fb.pitch;
        columns = fb.width / FONT_WIDTH;
        rows = fb.height / FONT_HEIGHT;

        fg_color = WHITE;

        cursor_blink_event = devices::pit::create_timer(500);

        // Clear screen
        const auto line_pixels = memory::heap::alloc_array<uint32_t>(pitch);

        for (auto x = 0u; x < fb.width; x++) {
            line_pixels[x] = 0xFF000000;
        }

        for (auto y = 0u; y < fb.height; y++) {
            fbdev->ops->seek(fbdev, vfs::SeekType::Start, y * pitch * 4);
            fbdev->ops->write(fbdev, line_pixels, pitch * 4);
        }

        memory::heap::free(line_pixels);

        // Initialize cwd to root
        cwd = const_cast<char*>("/");
        cwd_heap_allocated = false;

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

        scheduler::exit(0);
    }

    void print(const char ch) {
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

            fbdev->ops->seek(fbdev, vfs::SeekType::Start, ((base_y + y) * pitch + base_x) * 4);
            fbdev->ops->write(fbdev, line_pixels, FONT_WIDTH * 4);
        }
    }

    void fill_cell(const uint32_t pixel) {
        const auto base_x = row * FONT_WIDTH;
        const auto base_y = column * FONT_HEIGHT;

        uint32_t line_pixels[FONT_WIDTH];

        for (auto x = 0u; x < FONT_WIDTH; x++) {
            line_pixels[x] = pixel;
        }

        for (auto y = 0u; y < FONT_HEIGHT; y++) {
            fbdev->ops->seek(fbdev, vfs::SeekType::Start, ((base_y + y) * pitch + base_x) * 4);
            fbdev->ops->write(fbdev, line_pixels, FONT_WIDTH * 4);
        }
    }

    void new_line() {
        column++;

        if (column >= rows) {
            const auto row_size = FONT_HEIGHT * pitch * 4;
            const auto row_pixels = memory::heap::alloc_array<uint8_t>(row_size);

            for (auto y = 1u; y < rows; y++) {
                fbdev->ops->seek(fbdev, vfs::SeekType::Start, y * row_size);
                fbdev->ops->read(fbdev, row_pixels, row_size);

                fbdev->ops->seek(fbdev, vfs::SeekType::Start, (y - 1) * row_size);
                fbdev->ops->write(fbdev, row_pixels, row_size);
            }

            memory::heap::free(row_pixels);
            column--;

            for (auto x = 0u; x < columns; x++) {
                row = x;
                fill_cell(0xFF000000);
            }
        }

        row = 0;
    }

    void set_color(const Color color) {
        fg_color = color;
    }

    void print(const char* str, const uint64_t len) {
        for (auto i = 0u; i < len; i++) {
            const auto ch = str[i];

            if (ch == '\n') {
                new_line();
            } else if (ch != '\0') {
                print(ch);

                row++;
                if (row >= columns) new_line();
            }
        }
    }

    void print(const char* str) {
        const auto len = utils::strlen(str);
        print(str, len);
    }

    // ReSharper disable once CppParameterMayBeConst
    void printf(const char* fmt, va_list args) {
        static char buffer[256];
        const auto length = npf_vsnprintf(buffer, 256, fmt, args);
        print(buffer, length);
    }

    bool get_char_from_event(const devices::keyboard::Event event, char& ch) {
        using Key = devices::keyboard::Key;

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
        using namespace devices::keyboard;

        const auto kbdev = vfs::open_file("/dev/keyboard", vfs::Mode::Read);
        const auto kb_event = kbdev->ops->ioctl(kbdev, IOCTL_CREATE_EVENT, 0);
        kbdev->ops->ioctl(kbdev, IOCTL_RESET_BUFFER, 0);

        auto size = 0u;

        for (;;) {
            fill_cell(cursor_visible ? 0xFFFFFFFF : 0xFF000000);

            scheduler::EventHandle handles[] = { cursor_blink_event, kb_event };
            const auto signalled = scheduler::wait_on_events(handles, 2, true);

            if (signalled & 0b01) {
                cursor_visible = !cursor_visible;
            }

            if (signalled & 0b10) {
                Event event;
                auto exit = false;

                while (kbdev->ops->read(kbdev, &event, sizeof(Event)) != 0) {
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

                        print(&ch, 1);
                    }
                }

                if (exit) break;
            }
        }

        scheduler::destroy_event(kb_event);
        vfs::close_file(kbdev);

        if (cursor_visible) fill_cell(0xFF000000);

        print("\n", 1);
        buffer[size] = '\0';
    }
} // namespace cosmos::shell
