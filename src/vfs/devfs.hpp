#pragma once
#include "vfs.hpp"

namespace cosmos::vfs::devfs {
    void create(Fs* fs);

    void register_device(void* handle, stl::StringView name, const FileOps* ops);

    inline void register_device(const Fs* fs, const stl::StringView name, const FileOps* ops) {
        register_device(fs->handle, name, ops);
    }

} // namespace cosmos::vfs::devfs
