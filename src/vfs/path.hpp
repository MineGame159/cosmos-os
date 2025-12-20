#pragma once

#include "stl/string_view.hpp"

#include <cstdint>

namespace cosmos::vfs {
    uint32_t check_abs_path(stl::StringView path);

    // Returns a heap-allocated path (caller must free with memory::heap::free)
    stl::StringView join(stl::StringView a, stl::StringView b);

    /// Returns a heap-allocated absolute path (caller must free with memory::heap::free)
    /// On error (invalid path, attempts to escape root, etc.) empty string (still needs to be freed).
    stl::StringView resolve(stl::StringView cwd, stl::StringView path);
} // namespace cosmos::vfs
