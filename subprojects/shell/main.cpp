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

extern "C" void _start() {
    terminal::init();

    for (;;) {
        print_prompt();

        // Read prompt
        char prompt_buffer[128];
        const auto size = terminal::read(prompt_buffer, 128);

        const auto prompt = stl::StringView(prompt_buffer, size);

        // Get command
        const auto space_index = prompt.index_of(' ');
        const auto name = space_index != -1 ? prompt.substr(0, space_index) : prompt;

        const auto cmd_fn = get_command_fn(name);

        if (cmd_fn == nullptr) {
            terminal::print(RED, "Unknown command\n");
            continue;
        }

        // Run command
        const auto args = space_index != -1 ? prompt.substr(space_index + 1) : "";
        cmd_fn(args);
    }

    sys::exit(0);
}
