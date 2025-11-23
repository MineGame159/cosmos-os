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

    // Resolve a possibly-relative path against a current working directory.
    // Returns a heap-allocated absolute path (caller must free with memory::heap::free).
    // On error (invalid path, attempts to escape root, etc.) returns nullptr.
    char* resolve_path(const char* cwd, const char* path);
} // namespace cosmos::vfs
