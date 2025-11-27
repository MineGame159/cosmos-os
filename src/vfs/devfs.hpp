#pragma once
#include "vfs.hpp"

namespace cosmos::vfs::devfs {
    struct Device {
        uint64_t (*read)(uint64_t offset, void* buffer, uint64_t length);
        uint64_t (*write)(uint64_t offset, const void* buffer, uint64_t length);
    };

    void create(Fs* fs);

    void register_device(void* handle, stl::StringView name, Device device);

    inline void register_device(const Fs* fs, const stl::StringView name, const Device device) {
        register_device(fs->handle, name, device);
    }

} // namespace cosmos::vfs::devfs
