#include "commands.hpp"

#include "memory/heap.hpp"
#include "memory/physical.hpp"
#include "shell.hpp"
#include "utils.hpp"
#include "vfs/vfs.hpp"
#include "vfs/path.hpp"

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
            const auto out = static_cast<char*>(memory::heap::alloc(len + 1));
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

    void meminfo([[maybe_unused]] const char* args) {
        print("Total");
        print(GRAY, ": ");
        printf("%d", static_cast<uint64_t>(memory::phys::get_total_pages()) * 4096 / 1024 / 1024);
        print(GRAY, " mB\n");

        print("Free");
        print(GRAY, ": ");
        printf("%d", static_cast<uint64_t>(memory::phys::get_free_pages()) * 4096 / 1024 / 1024);
        print(GRAY, " mB\n");
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

        const auto file = vfs::open_file(resolved, vfs::Mode::Write);

        if (file == nullptr) {
            print(RED, "Failed to open file\n");
            memory::heap::free(resolved);
            return;
        }

        const auto data = utils::str_trim_left(&args[path_length]);
        const auto data_length = utils::strlen(data);

        file->ops->write(file->handle, data, data_length);
        vfs::close_file(file);

        memory::heap::free(resolved);
    }

    void cat(const char* args) {
        char* resolved = resolve_or_default(args);
        if (resolved == nullptr) return;

        const auto file = vfs::open_file(resolved, vfs::Mode::Read);

        if (file == nullptr) {
            print(RED, "Failed to open file\n");
            memory::heap::free(resolved);
            return;
        }

        const auto length = file->ops->seek(file->handle, vfs::SeekType::End, 0);
        file->ops->seek(file->handle, vfs::SeekType::Start, 0);

        const auto str = static_cast<char*>(memory::heap::alloc(length + 1));
        file->ops->read(file->handle, str, length);
        str[length] = '\0';
        vfs::close_file(file);

        print(str);
        print("\n");

        memory::heap::free(str);
        memory::heap::free(resolved);
    }

    void ls(const char* args) {
        char* resolved = resolve_or_default(args);
        if (resolved == nullptr) return;

        const auto dir = vfs::open_dir(resolved);

        if (dir == nullptr) {
            print(RED, "Failed to open directory\n");
            memory::heap::free(resolved);
            return;
        }

        const char* child;

        while ((child = dir->ops->read(dir->handle)) != nullptr) {
            print(child);
            print("\n");
        }

        vfs::close_dir(dir);
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

        const auto dir = vfs::open_dir(resolved);
        if (dir == nullptr) {
            print(RED, "Not a directory\n");
            memory::heap::free(resolved);
            return;
        }

        vfs::close_dir(dir);

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

        if (!vfs::make_dir(resolved)) {
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

    static constexpr Command commands[] = {
        { "meminfo", "Display memory information", meminfo },
        { "touch", "Creates and writes a file", touch },
        { "cat", "Reads a file", cat },
        { "ls", "Lists children of a directory", ls },
        { "mkdir", "Create directory", mkdir_cmd },
        { "pwd", "Print working directory", pwd },
        { "rm", "Remove file or empty directory", rm_cmd },
        { "rmdir", "Remove empty directory", rmdir_cmd },
        { "cd", "Change directory", cd },
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
