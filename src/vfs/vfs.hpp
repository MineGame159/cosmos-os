#pragma once

#include "types.hpp"

namespace cosmos::vfs {
    using FsInitFn = bool (*)(Node* node, stl::StringView device_path);

    void register_filesystem(stl::StringView name, std::size_t additional_root_node_size, FsInitFn init_fn);

    Node* mount(stl::StringView target_path, stl::StringView filesystem_name, stl::StringView device_path);
    bool unmount(stl::StringView path);

    bool stat(stl::StringView path, Stat& stat);

    File* open(stl::StringView path, Mode mode);
    File* duplicate(File* file);
    void close(File* file);

    bool create_dir(stl::StringView path);

    bool remove(stl::StringView path);
} // namespace cosmos::vfs
