#include "ramfs.hpp"

#include "memory/heap.hpp"
#include "path.hpp"
#include "utils.hpp"

namespace cosmos::vfs::ramfs {
    struct Node;

    struct NodeFolder {
        Node* children_head;
        Node* children_tail;
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
            NodeFolder folder;
            NodeFile file;
        };

        Node* find_child(const char* child_name, const uint32_t child_name_length) const {
            if (is_file) return nullptr;
            auto child = folder.children_head;

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

            if (folder.children_head == nullptr) {
                folder.children_head = node;
                folder.children_tail = node;
            } else {
                folder.children_tail->next_child = node;
                folder.children_tail = node;
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

    Node* alloc_node(const char* name, const uint32_t name_length) {
        const auto node = static_cast<Node*>(memory::heap::alloc(sizeof(Node) + name_length + 1, alignof(Node)));
        utils::memset(node, 0, sizeof(Node) + name_length + 1);

        node->name = reinterpret_cast<const char*>(node + 1);
        node->name_length = name_length;
        utils::memcpy(const_cast<char*>(node->name), name, name_length);

        return node;
    }

    File* fs_open(void* handle, const char* path, const Mode mode) {
        Node* prev = nullptr;
        auto node = static_cast<Node*>(handle);
        auto it = iterate_path_entries(path);

        while (it.next()) {
            prev = node;

            if (node != nullptr) {
                node = node->find_child(it.entry, it.length);
            } else {
                node = nullptr;
            }
        }

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

    static constexpr FsOps fs_ops = {
        .open = fs_open,
    };

    void create(Fs* fs) {
        fs->handle = alloc_node("/", 1);
        fs->ops = &fs_ops;
    }
} // namespace cosmos::vfs::ramfs
