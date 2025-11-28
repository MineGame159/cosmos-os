#include "devfs.hpp"

#include "memory/heap.hpp"
#include "stl/linked_list.hpp"
#include "utils.hpp"

namespace cosmos::vfs::devfs {
    struct Node {
        stl::StringView name;
        const FileOps* ops;
    };

    // DirOps

    stl::StringView dir_read(Directory* dir) {
        const auto current = reinterpret_cast<stl::LinkedList<Node>::Node*>(dir->cursor);

        if (current != nullptr) {
            dir->cursor = reinterpret_cast<uint64_t>(current->next);
            return current->item.name;
        }

        return "";
    }

    static constexpr DirOps dir_ops = {
        .read = dir_read,
    };

    // FsOps

    File* fs_open_file(Fs* fs, stl::StringView path, const Mode mode) {
        const auto nodes = static_cast<stl::LinkedList<Node>*>(fs->handle);
        path = path.substr(path[0] == '/' ? 1 : 0);

        for (const auto node : *nodes) {
            if (node->name == path) {
                const auto file = memory::heap::alloc<File>();
                file->fs = fs;
                file->ops = node->ops;
                file->handle = node;
                file->mode = mode;
                file->cursor = 0;

                return file;
            }
        }

        return nullptr;
    }

    void fs_close_file([[maybe_unused]] Fs* fs, [[maybe_unused]] File* file) {
        memory::heap::free(file);
    }

    Directory* fs_open_dir(Fs* fs, const stl::StringView path) {
        if (path != "/") return nullptr;

        const auto nodes = static_cast<stl::LinkedList<Node>*>(fs->handle);

        const auto it = memory::heap::alloc<stl::LinkedList<Node>::Iterator>();
        *it = nodes->begin();

        const auto dir = memory::heap::alloc<Directory>();
        dir->fs = fs;
        dir->ops = &dir_ops;
        dir->handle = nullptr;
        dir->cursor = reinterpret_cast<uint64_t>(nodes->head);

        return dir;
    }

    void fs_close_dir([[maybe_unused]] Fs* fs, Directory* dir) {
        memory::heap::free(dir);
    }

    bool fs_make_dir([[maybe_unused]] Fs* fs, [[maybe_unused]] stl::StringView path) {
        return false;
    }

    bool fs_remove([[maybe_unused]] Fs* fs, [[maybe_unused]] stl::StringView path) {
        return false;
    }

    static constexpr FsOps fs_ops = {
        .open_file = fs_open_file,
        .close_file = fs_close_file,
        .open_dir = fs_open_dir,
        .close_dir = fs_close_dir,
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

    void register_device(void* handle, stl::StringView name, const FileOps* ops) {
        if (name.contains("/")) return;
        name = name.trim();

        const auto nodes = static_cast<stl::LinkedList<Node>*>(handle);
        const auto node = nodes->push_back_alloc(name.size() + 1);

        node->name = stl::StringView(reinterpret_cast<char*>(node + 1), name.size());
        utils::memcpy(const_cast<char*>(node->name.data()), name.data(), name.size());
        const_cast<char*>(node->name.data())[name.size()] = '\0';

        node->ops = ops;
    }
} // namespace cosmos::vfs::devfs
