#pragma once

#include "types.hpp"

namespace cosmos::vfs {
    using FsInitFn = bool (*)(Node* node, stl::StringView device_path);

    void register_filesystem(stl::StringView name, std::size_t additional_root_node_size, FsInitFn init_fn);

    Node* mount(stl::StringView target_path, stl::StringView filesystem_name, stl::StringView device_path);
    bool unmount(stl::StringView path);

    File* open_file(stl::StringView path, Mode mode);
    void close_file(File* file);

    void* open_dir(stl::StringView path);
    stl::StringView read_dir(void* dir);
    void close_dir(void* dir);

    bool create_dir(stl::StringView path);

    bool remove(stl::StringView path);
} // namespace cosmos::vfs
