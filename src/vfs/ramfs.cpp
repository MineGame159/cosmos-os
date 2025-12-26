#include "ramfs.hpp"

#include "memory/heap.hpp"
#include "stl/linked_list.hpp"
#include "stl/string_view.hpp"
#include "stl/utils.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "vfs.hpp"

namespace cosmos::vfs::ramfs {
    struct FileInfo {
        uint8_t* data;
        uint64_t data_capacity;
        uint64_t data_size;
    };

    // FileOps

    uint64_t file_seek(const stl::Rc<File>& file, const SeekType type, const int64_t offset) {
        const auto info = reinterpret_cast<FileInfo*>(file->node + 1);

        file->seek(info->data_size, type, offset);
        return file->cursor;
    }

    uint64_t file_read(const stl::Rc<File>& file, void* buffer, const uint64_t length) {
        const auto info = reinterpret_cast<FileInfo*>(file->node + 1);

        if (file->mode == Mode::Write) return 0;
        if (info->data == nullptr) return 0;
        if (file->cursor >= info->data_size) return 0;

        auto size = info->data_size - file->cursor;
        if (size > length) size = length;

        if (size > 0) {
            utils::memcpy(buffer, &info->data[file->cursor], size);
            file->cursor += size;
        }

        return size;
    }

    uint64_t file_write(const stl::Rc<File>& file, const void* buffer, const uint64_t length) {
        const auto info = reinterpret_cast<FileInfo*>(file->node + 1);
        if (file->mode == Mode::Read) return 0;

        if (file->cursor + length >= info->data_capacity) {
            const auto new_capacity = stl::max(info->data_capacity * 2, file->cursor + length);

            const auto new_data = memory::heap::alloc_array<uint8_t>(new_capacity);
            if (new_data == nullptr) return 0;

            if (info->data != nullptr) {
                utils::memcpy(new_data, info->data, info->data_size);
                memory::heap::free(info->data);
            }

            info->data = new_data;
            info->data_capacity = new_capacity;
        }

        utils::memcpy(&info->data[file->cursor], buffer, length);
        file->cursor += length;

        if (file->cursor > info->data_size) {
            info->data_size = file->cursor;
        }

        return length;
    }

    uint64_t file_ioctl([[maybe_unused]] const stl::Rc<File>& file, [[maybe_unused]] uint64_t op, [[maybe_unused]] uint64_t arg) {
        return IOCTL_UNKNOWN;
    }

    static constexpr FileOps file_ops = {
        .seek = file_seek,
        .read = file_read,
        .write = file_write,
        .ioctl = file_ioctl,
    };

    // FsOps

    Node* fs_create(Node* parent, const NodeType type, const stl::StringView name) {
        Node* node;

        if (type == NodeType::Directory) {
            node = parent->children.push_back_alloc(name.size() + 1);
            node->name = stl::StringView(reinterpret_cast<const char*>(node + 1), name.size());
            node->populated = true;
        } else {
            node = parent->children.push_back_alloc(sizeof(FileInfo) + name.size() + 1);
            node->name = stl::StringView(reinterpret_cast<const char*>(node) + sizeof(Node) + sizeof(FileInfo), name.size());
            node->populated = false;

            const auto info = reinterpret_cast<FileInfo*>(node + 1);
            utils::memset(info, 0, sizeof(FileInfo));
        }

        node->parent = parent;
        node->mount_root = false;
        node->type = type;
        node->fs_ops = parent->fs_ops;
        node->fs_handle = parent->fs_handle;
        node->open_read = 0;
        node->open_write = 0;
        node->children = {};

        utils::memcpy(const_cast<char*>(node->name.data()), name.data(), name.size());
        const_cast<char*>(node->name.data())[name.size()] = '\0';

        return node;
    }

    bool fs_destroy(Node* node) {
        for (auto it = node->parent->children.begin(); it != stl::LinkedList<Node>::end(); ++it) {
            if (*it == node) {
                if (node->type == NodeType::File) {
                    const auto info = reinterpret_cast<FileInfo*>(node + 1);
                    if (info->data != nullptr) memory::heap::free(info->data);
                }

                node->parent->children.remove_free(it);
                return true;
            }
        }

        return false;
    }

    void fs_populate(Node* node) {
        node->populated = true;
    }

    const FileOps* fs_open(const Node* node, Mode mode) {
        return &file_ops;
    }

    void fs_on_close([[maybe_unused]] const File* file) {}

    static constexpr FsOps fs_ops = {
        .create = fs_create,
        .destroy = fs_destroy,
        .populate = fs_populate,
        .open = fs_open,
        .on_close = fs_on_close,
    };

    // Header

    bool init(Node* node, [[maybe_unused]] stl::StringView device_path) {
        node->fs_ops = &fs_ops;
        node->fs_handle = nullptr;

        node->populated = true;

        return true;
    }

    void register_filesystem() {
        vfs::register_filesystem("ramfs", 0, init);
    }
} // namespace cosmos::vfs::ramfs
