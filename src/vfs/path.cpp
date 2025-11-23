#include "path.hpp"

namespace cosmos::vfs {
    uint32_t check_abs_path(const char* path) {
        if (*path != '/') return 0;
        auto length = 1u;

        while (path[length] != '\0') {
            if (path[length] == '/' && path[length - 1] == '/') {
                return 0;
            }

            if (path[length] == ' ' && (path[length - 1] == '/' || path[length + 1] == '/' || path[length + 1] == '\0')) {
                return 0;
            }

            length++;
        }

        if (length > 1 && path[length - 1] == '/') {
            length--;
        }

        return length;
    }

    bool PathEntryIt::next() {
        const auto prev_entry = entry;
        entry += length;

        while (*entry == '/') {
            entry++;
        }

        if (*entry == '\0') {
            entry = prev_entry;
            return false;
        }

        length = 0;
        while (entry[length] != '/' && entry[length] != '\0') {
            length++;
        }

        return true;
    }

    PathEntryIt iterate_path_entries(const char* path) {
        return {
            .entry = path,
            .length = 0,
        };
    }
} // namespace cosmos::vfs
