#include "ramfs.hpp"

#include "memory/heap.hpp"
#include "path.hpp"
#include "utils.hpp"

namespace cosmos::vfs::ramfs {
    struct Node;

    struct NodeDir {
        Node* children_head;
        Node* children_tail;

        bool opened;
        Node* child;
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
        Node* next_child;

        const char* name;
        uint32_t name_length;

        bool is_file;

        union {
            NodeDir dir;
            NodeFile file;
        };

        Node* find_child(const char* child_name, const uint32_t child_name_length) const {
            if (is_file) return nullptr;
            auto child = dir.children_head;

            while (child != nullptr) {
                if (utils::streq(child->name, child->name_length, child_name, child_name_length)) {
                    return child;
                }

                child = child->next_child;
            }

            return nullptr;
        }

        void add_child(Node* node) {
            if (is_file) return;

            if (dir.children_head == nullptr) {
                dir.children_head = node;
                dir.children_tail = node;
            } else {
                dir.children_tail->next_child = node;
                dir.children_tail = node;
            }
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

    const char* dir_read(void* handle) {
        const auto node = static_cast<Node*>(handle);

        if (node->dir.child != nullptr) {
            const auto child = node->dir.child;
            node->dir.child = child->next_child;
            return child->name;
        }

        return nullptr;
    }

    void dir_close(void* handle) {
        const auto node = static_cast<Node*>(handle);
        node->dir.opened = false;
    }

    static constexpr DirOps dir_ops = {
        .read = dir_read,
        .close = dir_close,
    };

    Node* alloc_node(const char* name, const uint32_t name_length) {
        const auto node = static_cast<Node*>(memory::heap::alloc(sizeof(Node) + name_length + 1, alignof(Node)));
        utils::memset(node, 0, sizeof(Node) + name_length + 1);

        node->name = reinterpret_cast<const char*>(node + 1);
        node->name_length = name_length;
        utils::memcpy(const_cast<char*>(node->name), name, name_length);

        return node;
    }

    Node* find_node(void* handle, const char* path, Node*& prev, PathEntryIt& it) {
        auto node = static_cast<Node*>(handle);
        it = iterate_path_entries(path);

        while (it.next()) {
            prev = node;

            if (node != nullptr) {
                node = node->find_child(it.entry, it.length);
            } else {
                node = nullptr;
            }
        }

        return node;
    }

    File* fs_open_file(void* handle, const char* path, const Mode mode) {
        Node* prev = nullptr;
        PathEntryIt it;

        auto node = find_node(handle, path, prev, it);
        if (prev == nullptr) return nullptr;

        if (node == nullptr && (mode == Mode::Write || mode == Mode::ReadWrite)) {
            node = alloc_node(it.entry, it.length);
            node->is_file = true;

            prev->add_child(node);
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

    Directory* fs_open_dir(void* handle, const char* path) {
        Node* prev = nullptr;
        PathEntryIt it;

        const auto node = find_node(handle, path, prev, it);
        if (node == nullptr || node->is_file || node->dir.opened) return nullptr;

        node->dir.opened = true;
        node->dir.child = node->dir.children_head;

        const auto dir = memory::heap::alloc<Directory>();
        dir->handle = node;
        dir->ops = &dir_ops;

        return dir;
    }

    bool fs_make_dir(void* handle, const char* path) {
        if (path == nullptr) return false;

        // If requesting root, it's already present
        if (path[0] == '/' && path[1] == '\0') return true;

        // Work on subpath without leading '/'
        const char* p = path;
        if (*p == '/') p++;

        auto* node = static_cast<Node*>(handle);

        // Walk segments, create the final segment only if its parent exists.
        while (*p != '\0') {
            const char* seg = p;
            uint32_t seg_len = 0;
            while (p[seg_len] != '/' && p[seg_len] != '\0') seg_len++;

            // If this is the final segment
            if (p[seg_len] == '\0') {
                // Check if already exists
                const auto existing = node->find_child(seg, seg_len);
                if (existing != nullptr) {
                    // If it's a file, fail; if dir, succeed
                    return !existing->is_file;
                }

                // Create new directory node under current node
                const auto new_node = alloc_node(seg, seg_len);
                new_node->is_file = false;
                new_node->dir.children_head = nullptr;
                new_node->dir.children_tail = nullptr;
                new_node->dir.opened = false;
                new_node->dir.child = nullptr;

                node->add_child(new_node);
                return true;
            }

            // Not final segment: must already exist and be a directory
            const auto child = node->find_child(seg, seg_len);
            if (child == nullptr || child->is_file) return false;

            // Advance to next segment
            node = child;
            p += seg_len;
            while (*p == '/') p++;
        }

        return false;
    }

    bool fs_remove(void* handle, const char* path) {
        if (path == nullptr) return false;

        // Don't allow removing root
        if (path[0] == '/' && path[1] == '\0') return false;

        // Work on subpath without leading '/'
        const char* p = path;
        if (*p == '/') p++;

        auto node = static_cast<Node*>(handle);
        Node* parent = nullptr;

        // Walk to target node
        while (*p != '\0') {
            const char* seg = p;
            uint32_t seg_len = 0;
            while (p[seg_len] != '/' && p[seg_len] != '\0') seg_len++;

            const auto child = node->find_child(seg, seg_len);
            if (child == nullptr) return false;

            parent = node;
            node = child;

            if (p[seg_len] == '\0') break;
            p += seg_len;
            while (*p == '/') p++;
        }

        if (parent == nullptr) return false; // target has no parent, shouldn't happen except root

        // Remove file
        if (node->is_file) {
            if (node->file.opened) return false;

            if (node->file.data != nullptr) {
                memory::heap::free(node->file.data);
            }

            // unlink from parent's child list
            Node* cur = parent->dir.children_head;
            Node* prev = nullptr;
            while (cur != nullptr && cur != node) {
                prev = cur;
                cur = cur->next_child;
            }

            if (cur == nullptr) return false;

            if (prev == nullptr) parent->dir.children_head = cur->next_child;
            else prev->next_child = cur->next_child;

            if (parent->dir.children_tail == cur) parent->dir.children_tail = prev;

            memory::heap::free(cur);
            return true;
        }

        // Remove directory (must be empty and not opened)
        if (node->dir.opened) return false;
        if (node->dir.children_head != nullptr) return false;

        // unlink from parent
        Node* cur = parent->dir.children_head;
        Node* prev = nullptr;
        while (cur != nullptr && cur != node) {
            prev = cur;
            cur = cur->next_child;
        }

        if (cur == nullptr) return false;

        if (prev == nullptr) parent->dir.children_head = cur->next_child;
        else prev->next_child = cur->next_child;

        if (parent->dir.children_tail == cur) parent->dir.children_tail = prev;

        memory::heap::free(cur);
        return true;
    }

    static constexpr FsOps fs_ops = {
        .open_file = fs_open_file,
        .open_dir = fs_open_dir,
        .make_dir = fs_make_dir,
        .remove = fs_remove,
    };

    void create(Fs* fs) {
        fs->handle = alloc_node("/", 1);
        fs->ops = &fs_ops;
    }
} // namespace cosmos::vfs::ramfs
