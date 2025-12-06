#pragma once

#include "types.hpp"

namespace cosmos::vfs::devfs {
    void register_filesystem();

    void register_device(Node* node, stl::StringView name, const FileOps* ops, void* handle);
} // namespace cosmos::vfs::devfs
