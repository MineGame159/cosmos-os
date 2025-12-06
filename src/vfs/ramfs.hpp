#pragma once

#include "types.hpp"

namespace cosmos::vfs::ramfs {
    bool init(Node* node, stl::StringView device_path);
} // namespace cosmos::vfs::ramfs
