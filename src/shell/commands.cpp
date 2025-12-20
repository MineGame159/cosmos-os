#include "commands.hpp"

#include "memory/heap.hpp"
#include "scheduler/scheduler.hpp"
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

    static void free_string(const stl::StringView str) {
        memory::heap::free(const_cast<char*>(str.data()));
    }

    static bool resolve(const stl::StringView path, stl::StringView& abs_path) {
        abs_path = vfs::resolve(scheduler::get_cwd(scheduler::get_current_process()), path);

        if (abs_path.empty()) {
            print(RED, "Invalid path\n");
            free_string(abs_path);
            return false;
        }

        return true;
    }

    void touch(const stl::StringView args) {
        const auto space = args.index_of(' ');
        if (space == -1) {
            print(RED, "Needs path\n");
            return;
        }

        const auto path = args.substr(0, space);
        const auto text = args.substr(space + 1);

        stl::StringView abs_path;
        if (!resolve(path, abs_path)) return;

        const auto file = vfs::open(abs_path, vfs::Mode::Write);

        if (file == nullptr) {
            print(RED, "Failed to open file\n");
            free_string(abs_path);
            return;
        }

        file->ops->write(file, text.data(), text.size());
        vfs::close(file);

        free_string(abs_path);
    }

    void cat(const stl::StringView args) {
        stl::StringView abs_path;
        if (!resolve(args, abs_path)) return;

        const auto file = vfs::open(abs_path, vfs::Mode::Read);

        if (file == nullptr) {
            print(RED, "Failed to open file\n");
            free_string(abs_path);
            return;
        }

        char buffer[512];
        uint64_t read;

        while ((read = file->ops->read(file, buffer, 512)) != 0) {
            print(buffer, read);
        }

        print("\n");
        vfs::close(file);

        free_string(abs_path);
    }

    void ls(const stl::StringView args) {
        stl::StringView abs_path;
        if (!resolve(args, abs_path)) return;

        vfs::Stat stat;
        if (!vfs::stat(abs_path, stat)) {
            print(RED, "Not a directory\n");
            free_string(abs_path);
            return;
        }

        const auto dir = vfs::open(abs_path, vfs::Mode::Read);

        if (dir == nullptr) {
            print(RED, "Failed to open directory\n");
            free_string(abs_path);
            return;
        }

        vfs::DirEntry entry;

        while (dir->ops->read(dir, &entry, sizeof(vfs::DirEntry)) != 0) {
            print(entry.type == vfs::NodeType::Directory ? CYAN : WHITE, entry.name);
            print("\n");
        }

        vfs::close(dir);
        free_string(abs_path);
    }

    void cd(const stl::StringView args) {
        stl::StringView abs_path;
        if (!resolve(args, abs_path)) return;

        vfs::Stat stat;
        if (!vfs::stat(abs_path, stat) || stat.type != vfs::NodeType::Directory) {
            print(RED, "Not a directory\n");
            free_string(abs_path);
            return;
        }

        scheduler::set_cwd(scheduler::get_current_process(), abs_path);

        free_string(abs_path);
    }

    void help([[maybe_unused]] stl::StringView args);

    void mkdir_cmd(const stl::StringView args) {
        stl::StringView abs_path;
        if (!resolve(args, abs_path)) return;

        if (!vfs::create_dir(abs_path)) {
            print(RED, "Failed to create directory\n");
            free_string(abs_path);
            return;
        }

        free_string(abs_path);
    }

    void pwd([[maybe_unused]] const stl::StringView args) {
        const auto cwd = scheduler::get_cwd(scheduler::get_current_process());
        print(cwd.data(), cwd.size());
        print("\n");
    }

    void rm_cmd(const stl::StringView args) {
        stl::StringView abs_path;
        if (!resolve(args, abs_path)) return;

        if (!vfs::remove(abs_path)) {
            print(RED, "Failed to remove\n");
            free_string(abs_path);
            return;
        }

        free_string(abs_path);
    }

    void rmdir_cmd(const stl::StringView args) {
        // same as rm for now, ramfs remove only removes empty directories
        rm_cmd(args);
    }

    void mount_cmd(const stl::StringView args) {
        auto it = stl::split(args, ' ');

        // Target path
        if (!it.next()) {
            print(RED, "Missing target path\n");
            return;
        }

        stl::StringView target_path;
        if (!resolve(it.entry, target_path)) return;

        // Filesystem name
        if (!it.next()) {
            print(RED, "Missing filesystem name\n");
            free_string(target_path);
            return;
        }

        const auto filesystem_name = it.entry;

        // Device path
        auto device_path = stl::StringView("", 0);

        if (it.next()) {
            if (!resolve(it.entry, device_path)) return;
        }

        // Mount
        const auto node = vfs::mount(target_path, filesystem_name, device_path);

        if (node == nullptr) {
            print(RED, "Failed to mount filesystem\n");
        }

        // Free
        if (!device_path.empty()) free_string(device_path);
        free_string(target_path);
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

    void help([[maybe_unused]] const stl::StringView args) {
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
