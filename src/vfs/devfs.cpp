#include "devfs.hpp"

#include "memory/heap.hpp"
#include "stl/linked_list.hpp"
#include "utils.hpp"

namespace cosmos::vfs::devfs {
    struct Node {
        stl::StringView name;
        Device device;
    };

    // DirOps

    stl::StringView dir_read(void* handle) {
        const auto it = static_cast<stl::LinkedList<Node>::Iterator*>(handle);

        if (*it != stl::LinkedList<Node>::end()) {
            return ((*it)++)->name;
        }

        return "";
    }

    void dir_close(void* handle) {
        const auto it = static_cast<stl::LinkedList<Node>::Iterator*>(handle);
        memory::heap::free(it);
    }

    static constexpr DirOps dir_ops = {
        .read = dir_read,
        .close = dir_close,
    };

    // FsOps

    File* fs_open_file(void* handle, stl::StringView path, Mode mode) {
        return nullptr;
    }

    Directory* fs_open_dir(void* handle, const stl::StringView path) {
        if (path != "/") return nullptr;

        const auto nodes = static_cast<stl::LinkedList<Node>*>(handle);

        const auto it = memory::heap::alloc<stl::LinkedList<Node>::Iterator>();
        *it = nodes->begin();

        const auto dir = memory::heap::alloc<Directory>();
        dir->handle = it;
        dir->ops = &dir_ops;

        return dir;
    }

    bool fs_make_dir([[maybe_unused]] void* handle, [[maybe_unused]] stl::StringView path) {
        return false;
    }

    bool fs_remove([[maybe_unused]] void* handle, [[maybe_unused]] stl::StringView path) {
        return false;
    }

    static constexpr FsOps fs_ops = {
        .open_file = fs_open_file,
        .open_dir = fs_open_dir,
        .make_dir = fs_make_dir,
        .remove = fs_remove,
    };

    // Header

    void create(Fs* fs) {
        const auto nodes = memory::heap::alloc<stl::LinkedList<Node>>();
        *nodes = {};

        fs->handle = nodes;
        fs->ops = &fs_ops;
    }

    void register_device(void* handle, stl::StringView name, const Device device) {
        if (name.contains("/")) return;
        name = name.trim();

        const auto nodes = static_cast<stl::LinkedList<Node>*>(handle);
        const auto node = nodes->push_back_alloc(name.size());

        node->name = stl::StringView(reinterpret_cast<char*>(node + 1), name.size());
        utils::memcpy(const_cast<char*>(node->name.data()), name.data(), name.size());

        node->device = device;
    }
} // namespace cosmos::vfs::devfs
