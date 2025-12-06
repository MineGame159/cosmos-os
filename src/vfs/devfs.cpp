#include "devfs.hpp"

#include "utils.hpp"
#include "vfs.hpp"

namespace cosmos::vfs::devfs {
    // FsOps

    Node* fs_create([[maybe_unused]] Node* parent, [[maybe_unused]] NodeType type, [[maybe_unused]] stl::StringView name) {
        return nullptr;
    }

    bool fs_destroy([[maybe_unused]] Node* node) {
        return false;
    }

    void fs_populate(Node* node) {
        node->populated = true;
    }

    const FileOps* fs_open(const Node* node, const Mode mode) {
        const auto ops = *reinterpret_cast<FileOps* const*>(node + 1);

        if (is_read(mode) && ops->read == nullptr) return nullptr;
        if (is_write(mode) && ops->write == nullptr) return nullptr;

        return ops;
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
        vfs::register_filesystem("devfs", 0, init);
    }


    void register_device(Node* node, stl::StringView name, const FileOps* ops, void* handle) {
        if (name.contains("/")) return;
        name = name.trim();

        const auto device = node->children.push_back_alloc(sizeof(FileOps*) + name.size() + 1);
        utils::memset(device, 0, sizeof(Node));
        *reinterpret_cast<const FileOps**>(device + 1) = ops;

        device->parent = node;
        device->type = NodeType::File;
        device->name = stl::StringView(reinterpret_cast<char*>(device) + sizeof(Node) + sizeof(FileOps*), name.size());
        device->fs_ops = &fs_ops;
        device->fs_handle = handle;

        utils::memcpy(const_cast<char*>(device->name.data()), name.data(), name.size());
        const_cast<char*>(device->name.data())[name.size()] = '\0';
    }
} // namespace cosmos::vfs::devfs
