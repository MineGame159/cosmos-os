#pragma once

#include "types.hpp"

namespace cosmos::vfs::devfs {
    bool init(Node* node, stl::StringView device_path);

    void register_device(Node* node, stl::StringView name, const FileOps* ops, void* handle);
} // namespace cosmos::vfs::devfs
