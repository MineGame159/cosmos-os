#include "commands.hpp"

#include "syscalls.hpp"
#include "terminal.hpp"

#include <cstring>

struct Command {
    stl::StringView name;
    stl::StringView description;
    CommandFn fn;
};

#define CSTR(name)                                                                                                                         \
    const auto name##_cstr = static_cast<char*>(__builtin_alloca(name.size() + 1));                                                        \
    memcpy(name##_cstr, name.data(), name.size());                                                                                         \
    name##_cstr[name.size()] = '\0';

// Commands

static void ls(const stl::StringView args) {
    CSTR(args)

    // Check directory
    sys::Stat stat;

    if (!sys::stat(args_cstr, &stat) || stat.type != sys::FileType::Directory) {
        terminal::print(RED, "Not a directory\n");
        return;
    }

    // Open directory
    uint32_t fd;

    if (!sys::open(args_cstr, sys::Mode::Read, &fd)) {
        terminal::print(RED, "Failed to open directory\n");
        return;
    }

    // Read directory
    sys::DirEntry entry;
    uint64_t read;

    while (sys::read(fd, &entry, sizeof(sys::DirEntry), read) && read == sizeof(sys::DirEntry)) {
        const auto color = entry.type == sys::FileType::Directory ? CYAN : WHITE;

        terminal::print(color, stl::StringView(entry.name, entry.name_size));
        terminal::print("\n");
    }

    sys::close(fd);
}

static void cat(const stl::StringView args) {
    CSTR(args)

    // Check file
    sys::Stat stat;

    if (!sys::stat(args_cstr, &stat) || stat.type != sys::FileType::File) {
        terminal::print(RED, "Not a file\n");
        return;
    }

    // Open file
    uint32_t fd;

    if (!sys::open(args_cstr, sys::Mode::Read, &fd)) {
        terminal::print(RED, "Failed to open file\n");
        return;
    }

    // Read file
    char buffer[512];
    uint64_t read;

    while (sys::read(fd, buffer, 512, read) && read > 0) {
        terminal::print(stl::StringView(buffer, read));
    }

    terminal::print("\n");
    sys::close(fd);
}

static void touch(const stl::StringView args) {
    // Parse args
    const auto space_index = args.index_of(' ');

    const auto path = space_index != -1 ? args.substr(0, space_index) : args;
    const auto text = space_index != -1 ? args.substr(space_index + 1) : "";

    CSTR(path)

    // Open file
    uint32_t fd;

    if (!sys::open(path_cstr, sys::Mode::Write, &fd)) {
        terminal::print(RED, "Failed to open file\n");
        return;
    }

    // Write file
    sys::write(fd, text.data(), text.size());

    sys::close(fd);
}

static void mkdir(const stl::StringView args) {
    CSTR(args)

    if (!sys::create_dir(args_cstr)) {
        terminal::print(RED, "Failed to create directory\n");
    }
}

static void rm(const stl::StringView args) {
    CSTR(args)

    if (!sys::remove(args_cstr)) {
        terminal::print(RED, "Failed to remove file or directory\n");
    }
}

static void mount(const stl::StringView args) {
    auto it = stl::split(args, ' ');

    // Target path
    if (!it.next()) {
        terminal::print(RED, "Missing target path\n");
        return;
    }

    const auto target_path = it.entry;

    // Filesystem name
    if (!it.next()) {
        terminal::print(RED, "Missing filesystem name\n");
        return;
    }

    const auto filesystem_name = it.entry;

    // Device path
    auto device_path = stl::StringView("", 0);

    if (it.next()) {
        device_path = it.entry;
    }

    // Mount
    CSTR(target_path)
    CSTR(filesystem_name)
    CSTR(device_path)

    if (!sys::mount(target_path_cstr, filesystem_name_cstr, device_path_cstr)) {
        terminal::print(RED, "Failed to mount filesystem\n");
    }
}

static void pwd([[maybe_unused]] stl::StringView args) {
    char buffer[64];
    const auto size = sys::get_cwd(buffer, 64);

    if (size == 0) {
        terminal::print(RED, "Failed to get current working directory\n");
        return;
    }

    terminal::print(stl::StringView(buffer, size));
    terminal::print("\n");
}

static void cd(const stl::StringView args) {
    CSTR(args)

    if (!sys::set_cwd(args_cstr)) {
        terminal::print(RED, "Failed to set current working directory\n");
    }
}

// Other

static void help(stl::StringView args);

static constexpr Command COMMANDS[] = {
    { "ls", "Lists children of a directory", ls }, //
    { "cat", "Reads a file", cat },
    { "touch", "Creates and writes a file", touch },
    { "mkdir", "Create directory", mkdir },
    { "rm", "Remove file or empty directory", rm },
    { "mount", "Mounts a filesystem to a directory", mount },
    { "pwd", "Print working directory", pwd },
    { "cd", "Change directory", cd },
    { "help", "Display all available commands", help },
};

static void help([[maybe_unused]] const stl::StringView args) {
    for (auto i = 0u; i < sizeof(COMMANDS) / sizeof(Command); i++) {
        const auto& cmd = COMMANDS[i];

        terminal::print(cmd.name);
        terminal::printf(GRAY, " - %s\n", cmd.description);
    }
}

CommandFn get_command_fn(const stl::StringView name) {
    for (auto i = 0u; i < sizeof(COMMANDS) / sizeof(Command); i++) {
        const auto& cmd = COMMANDS[i];
        if (cmd.name == name) return cmd.fn;
    }

    return nullptr;
}
