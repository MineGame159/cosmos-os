#include "commands.hpp"

#include "memory/heap.hpp"
#include "shell.hpp"
#include "utils.hpp"
#include "vfs/path.hpp"
#include "vfs/vfs.hpp"

namespace cosmos::shell {
    struct Command {
        const char* name;
        const char* description;
        const CommandFn fn;
    };

    static char* resolve_or_default(const char* arg) {
        const char* target = utils::str_trim_left(arg);
        if (*target == '\0') {
            const auto cwd = get_cwd();
            const auto len = vfs::check_abs_path(cwd);
            if (len == 0) return nullptr;
            const auto out = memory::heap::alloc_array<char>(len + 1);
            utils::memcpy(out, cwd, len);
            out[len] = '\0';
            return out;
        }

        char* resolved = vfs::resolve_path(get_cwd(), target);
        if (!resolved) {
            print(RED, "Invalid path\n");
        }

        return resolved;
    }

    void touch(const char* args) {
        const auto space = utils::str_index_of(args, ' ');

        const auto path_length = space >= 0 ? space : utils::strlen(args);
        const auto path = utils::strdup(args, path_length);

        char* resolved = vfs::resolve_path(get_cwd(), path);
        memory::heap::free(path);

        if (resolved == nullptr) {
            print(RED, "Invalid path\n");
            return;
        }

        const auto file = vfs::open(resolved, vfs::Mode::Write);

        if (file == nullptr) {
            print(RED, "Failed to open file\n");
            memory::heap::free(resolved);
            return;
        }

        const auto data = utils::str_trim_left(&args[path_length]);
        const auto data_length = utils::strlen(data);

        file->ops->write(file, data, data_length);
        vfs::close(file);

        memory::heap::free(resolved);
    }

    void cat(const char* args) {
        char* resolved = resolve_or_default(args);
        if (resolved == nullptr) return;

        const auto file = vfs::open(resolved, vfs::Mode::Read);

        if (file == nullptr) {
            print(RED, "Failed to open file\n");
            memory::heap::free(resolved);
            return;
        }

        char buffer[512];
        uint64_t read;

        while ((read = file->ops->read(file, buffer, 512)) != 0) {
            print(buffer, read);
        }

        print("\n");
        vfs::close(file);

        memory::heap::free(resolved);
    }

    void ls(const char* args) {
        char* resolved = resolve_or_default(args);
        if (resolved == nullptr) return;

        vfs::Stat stat;
        if (!vfs::stat(resolved, stat)) {
            print(RED, "Not a directory\n");
            memory::heap::free(resolved);
            return;
        }

        const auto dir = vfs::open(resolved, vfs::Mode::Read);

        if (dir == nullptr) {
            print(RED, "Failed to open directory\n");
            memory::heap::free(resolved);
            return;
        }

        vfs::DirEntry entry;

        while (dir->ops->read(dir, &entry, sizeof(vfs::DirEntry)) != 0) {
            print(entry.type == vfs::NodeType::Directory ? CYAN : WHITE, entry.name);
            print("\n");
        }

        vfs::close(dir);
        memory::heap::free(resolved);
    }

    void cd(const char* args) {
        const char* target = utils::str_trim_left(args);
        if (*target == '\0') target = "/";

        char* resolved = vfs::resolve_path(get_cwd(), target);
        if (resolved == nullptr) {
            print(RED, "Invalid path\n");
            return;
        }

        vfs::Stat stat;
        if (!vfs::stat(resolved, stat) || stat.type != vfs::NodeType::Directory) {
            print(RED, "Not a directory\n");
            memory::heap::free(resolved);
            return;
        }

        if (!set_cwd(resolved)) {
            print(RED, "Failed to set cwd\n");
            memory::heap::free(resolved);
            return;
        }

        memory::heap::free(resolved);
    }

    void help([[maybe_unused]] const char* args);

    void mkdir_cmd(const char* args) {
        const auto path = utils::str_trim_left(args);
        if (*path == '\0') {
            print(RED, "Missing path\n");
            return;
        }

        char* resolved = vfs::resolve_path(get_cwd(), path);
        if (resolved == nullptr) {
            print(RED, "Invalid path\n");
            return;
        }

        if (!vfs::create_dir(resolved)) {
            print(RED, "Failed to create directory\n");
            memory::heap::free(resolved);
            return;
        }

        memory::heap::free(resolved);
    }

    void pwd([[maybe_unused]] const char* args) {
        const auto cwd = get_cwd();
        print(cwd);
        print("\n");
    }

    void rm_cmd(const char* args) {
        const auto path = utils::str_trim_left(args);
        if (*path == '\0') {
            print(RED, "Missing path\n");
            return;
        }

        char* resolved = vfs::resolve_path(get_cwd(), path);
        if (resolved == nullptr) {
            print(RED, "Invalid path\n");
            return;
        }

        if (!vfs::remove(resolved)) {
            print(RED, "Failed to remove\n");
            memory::heap::free(resolved);
            return;
        }

        memory::heap::free(resolved);
    }

    void rmdir_cmd(const char* args) {
        // same as rm for now, ramfs remove only removes empty directories
        rm_cmd(args);
    }

    char* resolve_path_view(const stl::StringView path) {
        const auto path_str = utils::strdup(path.data(), path.size());
        const auto resolved = vfs::resolve_path(get_cwd(), path_str);
        memory::heap::free(path_str);
        return resolved;
    }

    void mount_cmd(const char* args) {
        auto it = stl::split(args, ' ');

        // Target path
        if (!it.next()) {
            print(RED, "Missing target path\n");
            return;
        }

        const auto target_path = resolve_path_view(it.entry);

        // Filesystem name
        if (!it.next()) {
            print(RED, "Missing filesystem name\n");
            memory::heap::free(target_path);
            return;
        }

        const auto filesystem_name = it.entry;

        // Device path
        auto device_path = const_cast<char*>("");

        if (it.next()) {
            device_path = resolve_path_view(it.entry);
        }

        // Mount
        const auto node = vfs::mount(target_path, filesystem_name, device_path);

        if (node == nullptr) {
            print(RED, "Failed to mount filesystem\n");
        }

        // Free
        if (!utils::streq(device_path, "")) memory::heap::free(device_path);
        memory::heap::free(target_path);
    }

    static constexpr Command commands[] = {
        { "touch", "Creates and writes a file", touch },
        { "cat", "Reads a file", cat },
        { "ls", "Lists children of a directory", ls },
        { "mkdir", "Create directory", mkdir_cmd },
        { "pwd", "Print working directory", pwd },
        { "rm", "Remove file or empty directory", rm_cmd },
        { "rmdir", "Remove empty directory", rmdir_cmd },
        { "cd", "Change directory", cd },
        { "mount", "Mounts a filesystem to a directory", mount_cmd },
        { "help", "Display all available commands", help },
    };

    void help([[maybe_unused]] const char* args) {
        for (auto i = 0u; i < sizeof(commands) / sizeof(Command); i++) {
            const auto& cmd = commands[i];

            print(cmd.name);
            printf(GRAY, " - %s\n", cmd.description);
        }
    }

    CommandFn get_command_fn(const char* name, const uint32_t name_length) {
        for (auto i = 0u; i < sizeof(commands) / sizeof(Command); i++) {
            const auto& cmd = commands[i];
            if (utils::streq(name, name_length, cmd.name, utils::strlen(cmd.name))) return cmd.fn;
        }

        return nullptr;
    }
} // namespace cosmos::shell
