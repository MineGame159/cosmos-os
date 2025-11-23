#pragma once

#include <cstdint>

namespace cosmos::vfs {
    uint32_t check_abs_path(const char* path);

    struct PathEntryIt {
        const char* entry;
        uint32_t length;

        bool next();
    };

    PathEntryIt iterate_path_entries(const char* path);
} // namespace cosmos::vfs
