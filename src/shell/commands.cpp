#include "commands.hpp"

#include "memory/heap.hpp"
#include "memory/physical.hpp"
#include "shell.hpp"
#include "utils.hpp"
#include "vfs/vfs.hpp"

namespace cosmos::shell {
    struct Command {
        const char* name;
        const char* description;
        const CommandFn fn;
    };

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

        const auto file = vfs::open(path, vfs::Mode::Write);
        memory::heap::free(path);

        if (file == nullptr) {
            print(RED, "Failed to open file\n");
            return;
        }

        const auto data = utils::str_trim_left(&args[path_length]);
        const auto data_length = utils::strlen(data);

        file->ops->write(file->handle, data, data_length);
        vfs::close(file);
    }

    void cat(const char* args) {
        const auto file = vfs::open(args, vfs::Mode::Read);

        if (file == nullptr) {
            print(RED, "Failed to open file\n");
            return;
        }

        const auto length = file->ops->seek(file->handle, vfs::SeekType::End, 0);
        file->ops->seek(file->handle, vfs::SeekType::Start, 0);

        const auto str = static_cast<char*>(memory::heap::alloc(length + 1));
        file->ops->read(file->handle, str, length);
        str[length] = '\0';
        vfs::close(file);

        print(str);
        print("\n");

        memory::heap::free(str);
    }

    void help([[maybe_unused]] const char* args);

    static constexpr Command commands[] = {
        { "meminfo", "Display memory information", meminfo },
        { "touch", "Creates and writes a file", touch },
        { "cat", "Reads a file", cat },
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
