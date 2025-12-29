#include "commands.hpp"
#include "syscalls.hpp"
#include "terminal.hpp"

#include <cstring>

static uint64_t get_total_size(const stl::StringView* segments, const uint32_t segment_count) {
    uint64_t size = 0;

    for (auto i = 0u; i < segment_count; i++) {
        size += 1 + segments[i].size();
    }

    return size;
}

static void print_prompt() {
    constexpr uint64_t MAX = 32;
    char buffer[128];

    // Get CWD
    const auto size = sys::get_cwd(buffer, 128);

    // Print CWD directly if below max length
    if (size <= MAX) {
        terminal::print(DARK_CYAN, "[");
        terminal::print(CYAN, stl::StringView(buffer, size));
        terminal::print(DARK_CYAN, "]");
        terminal::print(WHITE, " > ");

        return;
    }

    // Split path to segments
    stl::StringView segments[32];
    uint32_t segment_count = 0;

    segments[segment_count++] = "...";

    auto it = stl::split(stl::StringView(buffer, size), '/');

    while (it.next()) {
        if (segment_count == 32) break;
        segments[segment_count++] = it.entry;
    }

    // Remove segments until below max length
    while (get_total_size(segments, segment_count) > MAX && segment_count >= 2) {
        memcpy(&segments[1], &segments[2], (segment_count - 2) * sizeof(stl::StringView));
        segment_count--;
    }

    // Print segments
    terminal::print(DARK_CYAN, "[");

    for (auto i = 0u; i < segment_count; i++) {
        terminal::print(CYAN, "/");
        terminal::print(CYAN, segments[i]);
    }

    terminal::print(DARK_CYAN, "]");
    terminal::print(WHITE, " > ");
}

enum class EscapeState {
    None,
    Command,
    ColorR,
    ColorG,
    ColorB,
};

static void run_command(const CommandFn fn, const stl::StringView args) {
    // Create out pipe
    uint32_t out_read_fd, out_write_fd;
    sys::pipe(sys::FileFlags::None, out_read_fd, out_write_fd);

    // Fork process
    uint32_t child_pid;
    sys::fork(child_pid);

    if (child_pid == 0) {
        // Setup out pipe
        sys::duplicate(out_write_fd, 1);
        sys::close(out_read_fd);
        sys::close(out_write_fd);

        // Run command
        fn(args);

        // Exit child process
        sys::exit(0);
    } else {
        // Close unneeded pipe ends
        sys::close(out_write_fd);
    }

    // Read out pipe and print to terminal
    char buffer[512];
    uint64_t read;

    auto state = EscapeState::None;
    uint8_t color_r = 0;
    uint8_t color_g = 0;

    terminal::set_fg_color(WHITE);

    while (sys::read(out_read_fd, buffer, 512, read) && read > 0) {
        for (auto i = 0u; i < read; i++) {
            const auto ch = buffer[i];

            switch (state) {
            case EscapeState::None:
                if (ch == 27) {
                    state = EscapeState::Command;
                } else {
                    terminal::print(ch);
                }
                break;
            case EscapeState::Command:
                switch (ch) {
                case 'f':
                    state = EscapeState::ColorR;
                    break;
                case 'r':
                    state = EscapeState::None;
                    terminal::set_fg_color(WHITE);
                    break;
                default:
                    state = EscapeState::None;
                    break;
                }
                break;
            case EscapeState::ColorR:
                state = EscapeState::ColorG;
                color_r = ch;
                break;
            case EscapeState::ColorG:
                state = EscapeState::ColorB;
                color_g = ch;
                break;
            case EscapeState::ColorB:
                state = EscapeState::None;
                terminal::set_fg_color({ color_r, color_g, static_cast<uint8_t>(ch) });
                break;
            }
        }
    }

    terminal::set_fg_color(WHITE);

    // Close rest of pipe ends
    sys::close(out_read_fd);

    // Join child process
    sys::join(child_pid);
}

static void run_file(const stl::StringView name) {
    CSTR(name)

    const char* args[2];
    args[0] = name_cstr;
    args[1] = nullptr;

    const char* env[1];
    env[0] = nullptr;

    sys::execute(name_cstr, args, env);

    // Print error message
    constexpr char fg_esc_seq[] = { 27, 'f', static_cast<char>(150), static_cast<char>(0), static_cast<char>(0) };
    constexpr stl::StringView msg = "Failed to execute file\n";

    sys::write(1, fg_esc_seq, sizeof(fg_esc_seq));
    sys::write(1, msg.data(), msg.size());
}

extern "C" [[noreturn]]
void _start() {
    // Fill first 3 FDs (stdio) with /dev/null
    uint32_t null_fd;

    sys::open("/dev/null", sys::Mode::Read, sys::FileFlags::None, null_fd);
    sys::open("/dev/null", sys::Mode::Write, sys::FileFlags::None, null_fd);
    sys::open("/dev/null", sys::Mode::Write, sys::FileFlags::None, null_fd);

    // Initialise terminal
    terminal::init();

    // Run shell
    for (;;) {
        print_prompt();

        // Read prompt
        char prompt_buffer[128];
        const auto size = terminal::read(prompt_buffer, 128);

        const auto prompt = stl::StringView(prompt_buffer, size);

        // Get command
        const auto space_index = prompt.index_of(' ');
        const auto name = space_index != -1 ? prompt.substr(0, space_index) : prompt;
        const auto args = space_index != -1 ? prompt.substr(space_index + 1) : "";

        const auto cmd_fn = get_command_fn(name);

        // Run command
        if (cmd_fn != nullptr) {
            run_command(cmd_fn, args);
            continue;
        }

        // Run file
        run_command(run_file, name);
    }

    sys::exit(0);
}
