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
        uint32_t open;
    };

    struct NodeFile {
        uint8_t* data;
        uint64_t data_capacity;
        uint64_t data_size;

        uint32_t open_read;
        uint32_t open_write;
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
            utils::memset(node, 0, sizeof(Node));
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

    // FileOps

    uint64_t file_seek(File* file, const SeekType type, const int64_t offset) {
        const auto node = static_cast<Node*>(file->handle);

        file->seek(node->file.data_size, type, offset);
        return file->cursor;
    }

    uint64_t file_read(File* file, void* buffer, const uint64_t length) {
        const auto node = static_cast<Node*>(file->handle);

        if (file->mode == Mode::Write) return 0;
        if (node->file.data == nullptr) return 0;
        if (file->cursor >= node->file.data_size) return 0;

        auto size = node->file.data_size - file->cursor;
        if (size > length) size = length;

        if (size > 0) {
            utils::memcpy(buffer, &node->file.data[file->cursor], size);
            file->cursor += size;
        }

        return size;
    }

    uint64_t file_write(File* file, const void* buffer, const uint64_t length) {
        const auto node = static_cast<Node*>(file->handle);
        if (file->mode == Mode::Read) return 0;

        if (file->cursor + length >= node->file.data_capacity) {
            const auto new_capacity = utils::max(node->file.data_capacity * 2, file->cursor + length);

            const auto new_data = static_cast<uint8_t*>(memory::heap::alloc(new_capacity));
            if (new_data == nullptr) return 0;

            if (node->file.data != nullptr) {
                utils::memcpy(new_data, node->file.data, node->file.data_size);
                memory::heap::free(node->file.data);
            }

            node->file.data = new_data;
            node->file.data_capacity = new_capacity;
        }

        utils::memcpy(&node->file.data[file->cursor], buffer, length);
        file->cursor += length;

        if (file->cursor > node->file.data_size) {
            node->file.data_size = file->cursor;
        }

        return length;
    }

    static constexpr FileOps file_ops = {
        .seek = file_seek,
        .read = file_read,
        .write = file_write,
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

    Node* find_node(const Fs* fs, const stl::StringView& path, Node*& prev, ViewPathEntryIt& it) {
        auto node = static_cast<Node*>(fs->handle);
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

    File* fs_open_file(Fs* fs, const stl::StringView path, const Mode mode) {
        Node* prev = nullptr;
        ViewPathEntryIt it;

        auto node = find_node(fs, path, prev, it);
        if (prev == nullptr) return nullptr;

        if (node == nullptr && is_write(mode)) {
            node = prev->alloc_child(it.entry);
            node->is_file = true;
        }

        if (node != nullptr) {
            if (!node->is_file) return nullptr;
            if (node->file.open_write > 0) return nullptr;
            if (is_write(mode) && node->file.open_read > 0) return nullptr;

            if (is_read(mode)) node->file.open_read++;
            if (is_write(mode)) node->file.open_write++;

            const auto file = memory::heap::alloc<File>();
            file->fs = fs;
            file->ops = &file_ops;
            file->handle = node;
            file->mode = mode;
            file->cursor = 0;

            return file;
        }

        return nullptr;
    }

    void fs_close_file([[maybe_unused]] Fs* fs, File* file) {
        const auto node = static_cast<Node*>(file->handle);

        if (is_read(file->mode)) node->file.open_read--;
        if (is_write(file->mode)) node->file.open_write--;

        memory::heap::free(file);
    }

    Directory* fs_open_dir(Fs* fs, const stl::StringView path) {
        Node* prev = nullptr;
        ViewPathEntryIt it;

        const auto node = find_node(fs, path, prev, it);
        if (node == nullptr || node->is_file) return nullptr;

        node->dir.open++;

        const auto dir = memory::heap::alloc<Directory>();
        dir->fs = fs;
        dir->ops = &dir_ops;
        dir->handle = node;
        dir->cursor = reinterpret_cast<uint64_t>(node->dir.children.head);

        return dir;
    }

    void fs_close_dir([[maybe_unused]] Fs* fs, Directory* dir) {
        const auto node = static_cast<Node*>(dir->handle);

        node->dir.open--;

        memory::heap::free(dir);
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    bool fs_make_dir(Fs* fs, const stl::StringView path) {
        if (path.empty()) return false;

        // If requesting root, it's already present
        if (path == "/") return false;

        Node* parent = nullptr;
        ViewPathEntryIt it;

        // Find the node corresponding to the final path segment (if it exists)
        const auto node = find_node(fs, path, parent, it);

        // parent should point to the parent directory of the final entry
        if (parent == nullptr) return false;

        // If node exists, succeed only if it's a directory
        if (node != nullptr) return !node->is_file;

        // Otherwise create a new directory node under parent using the final segment from 'it'
        const auto new_node = parent->alloc_child(it.entry);
        new_node->is_file = false;

        return true;
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    bool fs_remove(Fs* fs, const stl::StringView path) {
        if (path.empty()) return false;

        // Don't allow removing root
        if (path == "/") return false;

        Node* parent = nullptr;
        ViewPathEntryIt it;

        // Find the node to remove and its parent
        const auto node = find_node(fs, path, parent, it);
        if (node == nullptr || parent == nullptr) return false;

        // Remove file
        if (node->is_file) {
            if (node->file.open_read > 0 || node->file.open_write > 0) return false;

            if (node->file.data != nullptr) {
                memory::heap::free(node->file.data);
            }

            return parent->free_child(node);
        }

        // Remove directory (must be empty and not opened)
        if (node->dir.open > 0) return false;
        if (!node->dir.children.empty()) return false;

        return parent->free_child(node);
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
        const auto root = static_cast<Node*>(memory::heap::alloc(sizeof(Node) + 2, alignof(Node)));
        utils::memset(root, 0, sizeof(Node));
        root->init_name("/");

        fs->handle = root;
        fs->ops = &fs_ops;
    }
} // namespace cosmos::vfs::ramfs
