#include "vfs.hpp"

#include "log/log.hpp"
#include "memory/heap.hpp"
#include "path.hpp"
#include "stl/utils.hpp"
#include "utils.hpp"

namespace cosmos::vfs {
    struct Filesystem {
        stl::StringView name;
        std::size_t additional_root_node_size;
        FsInitFn init_fn;
    };

    static stl::LinkedList<Filesystem> filesystems = {};

    static Node* root = nullptr;

    static Node* find_node(const stl::StringView& path, Node*& parent, stl::SplitIterator& it) {
        parent = nullptr;
        auto node = root;

        it = stl::split(path, '/');

        while (it.next()) {
            auto found = false;

            if (node->type == NodeType::Directory) {
                if (!node->populated) node->fs_ops->populate(node);

                for (const auto child : node->children) {
                    if (child->name == it.entry) {
                        parent = node;
                        node = child;

                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                parent = node;
                return nullptr;
            }
        }

        return node;
    }

    void register_filesystem(const stl::StringView name, const std::size_t additional_root_node_size, const FsInitFn init_fn) {
        const auto filesystem = filesystems.push_back_alloc(name.size() + 1);

        filesystem->name = stl::StringView(reinterpret_cast<const char*>(filesystem + 1), name.size());
        filesystem->additional_root_node_size = additional_root_node_size;
        filesystem->init_fn = init_fn;

        utils::memcpy(const_cast<char*>(filesystem->name.data()), name.data(), name.size());
        const_cast<char*>(filesystem->name.data())[name.size()] = '\0';
    }

    static void init_mount_node(Node* node, Node* parent, const Filesystem* fs, const stl::StringView name) {
        utils::memset(node, 0, sizeof(Node));

        node->parent = parent;
        node->mount_root = true;
        node->type = NodeType::Directory;
        node->name = stl::StringView(reinterpret_cast<char*>(node + 1) + fs->additional_root_node_size, name.size());

        utils::memcpy(const_cast<char*>(node->name.data()), name.data(), name.size());
        const_cast<char*>(node->name.data())[name.size()] = '\0';
    }

    Node* mount(stl::StringView target_path, const stl::StringView filesystem_name, const stl::StringView device_path) {
        // Get filesystem
        const Filesystem* fs = nullptr;

        for (const auto filesystem : filesystems) {
            if (filesystem->name == filesystem_name) {
                fs = filesystem;
                break;
            }
        }

        if (fs == nullptr) return nullptr;

        // Check target path
        const auto length = check_abs_path(target_path);
        if (length == 0) return nullptr;
        target_path = target_path.substr(0, length);

        // Mount at /
        if (target_path == "/") {
            if (root != nullptr) return nullptr;

            root = static_cast<Node*>(memory::heap::alloc(sizeof(Node) + fs->additional_root_node_size + length + 1, alignof(Node)));
            init_mount_node(root, nullptr, fs, target_path.substr(0, length));

            if (!fs->init_fn(root, device_path)) {
                memory::heap::free(root);
                root = nullptr;

                return nullptr;
            }

            INFO("Mounted filesystem '%s' at %s", fs->name.data(), target_path.data());
            return root;
        }

        // Get parent directory node
        Node* parent;
        stl::SplitIterator it;
        auto node = find_node(target_path, parent, it);

        if (node != nullptr) return nullptr;
        if (parent->type != NodeType::Directory) return nullptr;
        if (it.next()) return nullptr;

        // Mount as child
        node = parent->children.push_back_alloc(fs->additional_root_node_size + it.entry.size() + 1);
        init_mount_node(node, parent, fs, it.entry);

        if (!fs->init_fn(node, device_path)) {
            for (auto child_it = parent->children.begin(); child_it != stl::LinkedList<Node>::end(); ++child_it) {
                if (*child_it == node) {
                    parent->children.remove_free(child_it);
                    break;
                }
            }

            return nullptr;
        }

        INFO("Mounted filesystem '%s' at %s", fs->name.data(), target_path.data());
        return node;
    }

    bool unmount(stl::StringView path) {
        const auto length = check_abs_path(path);
        if (length == 0) return false;
        path = path.substr(0, length);

        Node* parent;
        stl::SplitIterator it;
        const auto node = find_node(path, parent, it);

        if (parent == nullptr) return false;
        if (node == nullptr || !node->mount_root) return false;
        if (it.next()) return false;

        for (auto child_it = parent->children.begin(); child_it != stl::LinkedList<Node>::end(); ++child_it) {
            if (*child_it == node) {
                parent->children.remove_free(child_it);
                INFO("Unmounted filesystem at %s", path.data());

                return true;
            }
        }

        return false;
    }

    bool stat(stl::StringView path, Stat& stat) {
        const auto length = check_abs_path(path);
        if (length == 0) return false;
        path = path.substr(0, length);

        Node* parent;
        stl::SplitIterator it;
        const auto node = find_node(path, parent, it);

        if (node == nullptr) return false;

        stat = {
            .type = node->type,
        };

        return true;
    }

    static File* open_file(Node* node, const Mode mode) {
        const auto ops = node->fs_ops->open(node, mode);
        if (ops == nullptr) return nullptr;

        if (is_read(mode)) node->open_read++;
        if (is_write(mode)) node->open_write++;

        const auto file = memory::heap::alloc<File>();
        file->ops = ops;
        file->on_close = nullptr;
        file->node = node;
        file->ref_count = 1;
        file->mode = mode;
        file->cursor = 0;

        return file;
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    static uint64_t dir_seek(File* file, [[maybe_unused]] SeekType type, [[maybe_unused]] int64_t offset) {
        return file->cursor;
    }

    static uint64_t dir_read(File* file, void* buffer, const uint64_t length) {
        if (length != sizeof(DirEntry)) return 0;

        const auto it = reinterpret_cast<stl::LinkedList<Node>::Iterator*>(file + 1);

        if (*it != stl::LinkedList<Node>::end()) {
            const auto node = *it;
            const auto entry = static_cast<DirEntry*>(buffer);

            entry->type = node->type;
            utils::memcpy(entry->name, node->name.data(), stl::min(node->name.size(), 256ul));
            entry->name_size = node->name.size();

            ++*it;
            return sizeof(DirEntry);
        }

        return 0;
    }

    static uint64_t dir_write([[maybe_unused]] File* file, [[maybe_unused]] const void* buffer, [[maybe_unused]] uint64_t length) {
        return 0;
    }

    static uint64_t dir_ioctl([[maybe_unused]] File* file, [[maybe_unused]] uint64_t op, [[maybe_unused]] uint64_t arg) {
        return IOCTL_UNKNOWN;
    }

    static constexpr FileOps dir_ops = {
        .seek = dir_seek,
        .read = dir_read,
        .write = dir_write,
        .ioctl = dir_ioctl,
    };

    static File* open_dir(Node* node, const Mode mode) {
        if (is_write(mode)) return nullptr;

        if (!node->populated) node->fs_ops->populate(node);

        node->open_read++;

        const auto file = memory::heap::alloc<File>(sizeof(stl::LinkedList<Node>::Iterator));
        file->ops = &dir_ops;
        file->on_close = nullptr;
        file->node = node;
        file->ref_count = 1;
        file->mode = mode;
        file->cursor = 0;

        const auto it = reinterpret_cast<stl::LinkedList<Node>::Iterator*>(file + 1);
        *it = node->children.begin();

        return file;
    }

    File* open(stl::StringView path, const Mode mode) {
        const auto length = check_abs_path(path);
        if (length == 0) return nullptr;
        path = path.substr(0, length);

        Node* parent;
        stl::SplitIterator it;
        auto node = find_node(path, parent, it);

        if (node == nullptr && !it.next() && is_write(mode) && parent->type == NodeType::Directory) {
            node = parent->fs_ops->create(parent, NodeType::File, it.entry);
        }

        if (node != nullptr) {
            if (node->open_write > 0) return nullptr;
            if (is_write(mode) && node->open_read > 0) return nullptr;

            if (node->type == NodeType::Directory) return open_dir(node, mode);
            if (node->type == NodeType::File) return open_file(node, mode);

            return nullptr;
        }

        return nullptr;
    }

    File* duplicate(File* file) {
        file->ref_count++;
        return file;
    }

    void close(File* file) {
        if (file->ref_count == 0) {
            ERROR("File reference count is 0, double close detected");
            return;
        }

        if (file->ref_count > 1) {
            file->ref_count--;
            return;
        }

        if (file->node != nullptr) {
            if (is_read(file->mode)) file->node->open_read--;
            if (is_write(file->mode)) file->node->open_write--;

            file->node->fs_ops->on_close(file);
        }

        if (file->on_close != nullptr) {
            file->on_close(file);
        }

        memory::heap::free(file);
    }

    bool create_dir(stl::StringView path) {
        const auto length = check_abs_path(path);
        if (length == 0) return false;
        path = path.substr(0, length);

        Node* parent;
        stl::SplitIterator it;
        auto node = find_node(path, parent, it);

        if (node == nullptr && !it.next() && parent->type == NodeType::Directory) {
            node = parent->fs_ops->create(parent, NodeType::Directory, it.entry);
            return node != nullptr;
        }

        return false;
    }

    bool remove(stl::StringView path) {
        const auto length = check_abs_path(path);
        if (length == 0) return false;
        path = path.substr(0, length);

        Node* parent;
        stl::SplitIterator it;
        const auto node = find_node(path, parent, it);

        if (node == nullptr) return false;
        if (node->open_read > 0 || node->open_write > 0) return false;

        if (node->type == NodeType::Directory) {
            if (!node->populated) node->fs_ops->populate(node);
            if (!node->children.empty()) return false;
        }

        return node->fs_ops->destroy(node);
    }
} // namespace cosmos::vfs
