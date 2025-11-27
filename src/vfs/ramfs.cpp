#include "ramfs.hpp"

#include "memory/heap.hpp"
#include "path.hpp"
#include "stl/linked_list.hpp"
#include "stl/string_view.hpp"
#include "utils.hpp"

namespace cosmos::vfs::ramfs {
    struct Node;

    struct NodeDir {
        stl::LinkedList<Node> children;

        bool opened;
        stl::LinkedList<Node>::Iterator iterator;
    };

    struct NodeFile {
        uint8_t* data;
        uint64_t data_capacity;
        uint64_t data_size;

        bool opened;
        Mode mode;
        uint64_t cursor;
    };

    struct Node {
        stl::StringView name;
        bool is_file;

        union {
            NodeDir dir;
            NodeFile file;
        };

        void init_name(const stl::StringView new_name) {
            name = stl::StringView(reinterpret_cast<const char*>(this + 1), new_name.size());
            utils::memcpy(const_cast<char*>(name.data()), new_name.data(), new_name.size());
            const_cast<char*>(name.data())[name.size()] = '\0';
        }

        [[nodiscard]]
        Node* find_child(const stl::StringView child_name) const {
            if (is_file) return nullptr;

            for (const auto child : dir.children) {
                if (child->name == child_name) {
                    return child;
                }
            }

            return nullptr;
        }

        Node* alloc_child(const stl::StringView child_name) {
            if (is_file) return nullptr;

            const auto node = dir.children.push_back_alloc(child_name.size() + 1);
            node->init_name(child_name);

            return node;
        }

        bool free_child(const Node* child) {
            if (is_file) return false;

            for (auto it = dir.children.begin(); it != stl::LinkedList<Node>::end(); ++it) {
                if (*it == child) {
                    dir.children.remove_free(it);
                    return true;
                }
            }

            return false;
        }
    };

    uint64_t file_seek(void* handle, const SeekType type, const int64_t offset) {
        const auto node = static_cast<Node*>(handle);

        switch (type) {
        case SeekType::Start:
            node->file.cursor = offset;
            break;
        case SeekType::Current:
            node->file.cursor += offset;
            break;
        case SeekType::End:
            node->file.cursor = node->file.data_size + offset;
            break;
        }

        return node->file.cursor;
    }

    uint64_t file_read(void* handle, void* buffer, const uint64_t length) {
        const auto node = static_cast<Node*>(handle);

        if (node->file.mode == Mode::Write) return 0;
        if (node->file.data == nullptr) return 0;
        if (node->file.cursor >= node->file.data_size) return 0;

        auto size = node->file.data_size - node->file.cursor;
        if (size > length) size = length;

        if (size > 0) {
            utils::memcpy(buffer, &node->file.data[node->file.cursor], size);
            node->file.cursor += size;
        }

        return size;
    }

    uint64_t file_write(void* handle, const void* buffer, const uint64_t length) {
        const auto node = static_cast<Node*>(handle);
        if (node->file.mode == Mode::Read) return 0;

        if (node->file.cursor + length >= node->file.data_capacity) {
            const auto new_capacity = utils::max(node->file.data_capacity * 2, node->file.cursor + length);

            const auto new_data = static_cast<uint8_t*>(memory::heap::alloc(new_capacity));
            if (new_data == nullptr) return 0;

            if (node->file.data != nullptr) {
                utils::memcpy(new_data, node->file.data, node->file.data_size);
                memory::heap::free(node->file.data);
            }

            node->file.data = new_data;
            node->file.data_capacity = new_capacity;
        }

        utils::memcpy(&node->file.data[node->file.cursor], buffer, length);
        node->file.cursor += length;

        if (node->file.cursor > node->file.data_size) {
            node->file.data_size = node->file.cursor;
        }

        return length;
    }

    void file_close(void* handle) {
        const auto node = static_cast<Node*>(handle);
        node->file.opened = false;
    }

    static constexpr FileOps file_ops = {
        .seek = file_seek,
        .read = file_read,
        .write = file_write,
        .close = file_close,
    };

    constexpr stl::StringView EMPTY = "";

    const stl::StringView& dir_read(void* handle) {
        const auto node = static_cast<Node*>(handle);

        if (node->dir.iterator != stl::LinkedList<Node>::end()) {
            return (node->dir.iterator++)->name;
        }

        return EMPTY;
    }

    void dir_close(void* handle) {
        const auto node = static_cast<Node*>(handle);
        node->dir.opened = false;
    }

    static constexpr DirOps dir_ops = {
        .read = dir_read,
        .close = dir_close,
    };

    Node* find_node(void* handle, const stl::StringView& path, Node*& prev, ViewPathEntryIt& it) {
        auto node = static_cast<Node*>(handle);
        it = iterate_view_path_entries(path);

        while (it.next()) {
            prev = node;

            if (node != nullptr) {
                node = node->find_child(it.entry);
            } else {
                node = nullptr;
            }
        }

        return node;
    }

    File* fs_open_file(void* handle, const stl::StringView path, const Mode mode) {
        Node* prev = nullptr;
        ViewPathEntryIt it;

        auto node = find_node(handle, path, prev, it);
        if (prev == nullptr) return nullptr;

        if (node == nullptr && (mode == Mode::Write || mode == Mode::ReadWrite)) {
            node = prev->alloc_child(it.entry);
            node->is_file = true;
        }

        if (node != nullptr) {
            if (!node->is_file || node->file.opened) return nullptr;

            node->file.opened = true;
            node->file.mode = mode;
            node->file.cursor = 0;

            const auto file = memory::heap::alloc<File>();
            file->handle = node;
            file->ops = &file_ops;

            return file;
        }

        return nullptr;
    }

    Directory* fs_open_dir(void* handle, const stl::StringView path) {
        Node* prev = nullptr;
        ViewPathEntryIt it;

        const auto node = find_node(handle, path, prev, it);
        if (node == nullptr || node->is_file || node->dir.opened) return nullptr;

        node->dir.opened = true;
        node->dir.iterator = node->dir.children.begin();

        const auto dir = memory::heap::alloc<Directory>();
        dir->handle = node;
        dir->ops = &dir_ops;

        return dir;
    }

    bool fs_make_dir(void* handle, const stl::StringView path) {
        if (path.empty()) return false;

        // If requesting root, it's already present
        if (path == "/") return false;

        Node* parent = nullptr;
        ViewPathEntryIt it;

        // Find the node corresponding to the final path segment (if it exists)
        const auto node = find_node(handle, path, parent, it);

        // parent should point to the parent directory of the final entry
        if (parent == nullptr) return false;

        // If node exists, succeed only if it's a directory
        if (node != nullptr) return !node->is_file;

        // Otherwise create a new directory node under parent using the final segment from 'it'
        const auto new_node = parent->alloc_child(it.entry);
        new_node->is_file = false;
        new_node->dir.children = {};
        new_node->dir.opened = false;

        return true;
    }

    bool fs_remove(void* handle, const stl::StringView path) {
        if (path.empty()) return false;

        // Don't allow removing root
        if (path == "/") return false;

        Node* parent = nullptr;
        ViewPathEntryIt it;

        // Find the node to remove and its parent
        const auto node = find_node(handle, path, parent, it);
        if (node == nullptr || parent == nullptr) return false;

        // Remove file
        if (node->is_file) {
            if (node->file.opened) return false;

            if (node->file.data != nullptr) {
                memory::heap::free(node->file.data);
            }

            return parent->free_child(node);
        }

        // Remove directory (must be empty and not opened)
        if (node->dir.opened) return false;
        if (!node->dir.children.empty()) return false;

        return parent->free_child(node);
    }

    static constexpr FsOps fs_ops = {
        .open_file = fs_open_file,
        .open_dir = fs_open_dir,
        .make_dir = fs_make_dir,
        .remove = fs_remove,
    };

    void create(Fs* fs) {
        const auto root = static_cast<Node*>(memory::heap::alloc(sizeof(Node) + 2, alignof(Node)));
        root->init_name("/");
        root->is_file = false;
        root->dir.children = {};
        root->dir.opened = false;

        fs->handle = root;
        fs->ops = &fs_ops;
    }
} // namespace cosmos::vfs::ramfs
