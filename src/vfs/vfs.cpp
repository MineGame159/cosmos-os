#include "vfs.hpp"

#include "path.hpp"
#include "utils.hpp"

namespace cosmos::vfs {
    struct Filesystem {
        stl::StringView name;
        FsInitFn init_fn;
    };

    static stl::LinkedList<Filesystem> filesystems = {};

    static Node* root = nullptr;

    Node* find_node(const stl::StringView& path, Node*& parent, stl::SplitIterator& it) {
        parent = nullptr;
        auto node = root;

        it = stl::split(path, '/');

        while (it.next()) {
            auto found = false;

            if (node->type == NodeType::Directory) {
                if (!node->populated) node->fs_ops->populate(node->fs_handle, node);

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

    void init_mount_node(Node* node, Node* parent, const stl::StringView name) {
        utils::memset(node, 0, sizeof(Node));

        utils::memcpy(node + 1, name.data(), name.size());
        reinterpret_cast<char*>(node + 1)[name.size()] = '\0';

        node->parent = parent;
        node->type = NodeType::Directory;
        node->name = stl::StringView(reinterpret_cast<char*>(node + 1), name.size());
    }

    void register_filesystem(const stl::StringView name, const FsInitFn init_fn) {
        const auto filesystem = filesystems.push_back_alloc(name.size() + 1);

        filesystem->name = stl::StringView(reinterpret_cast<const char*>(filesystem + 1), name.size());
        filesystem->init_fn = init_fn;

        utils::memcpy(const_cast<char*>(filesystem->name.data()), name.data(), name.size());
        const_cast<char*>(filesystem->name.data())[name.size()] = '\0';
    }

    FsInitFn get_filesystem(const stl::StringView name) {
        for (const auto filesystem : filesystems) {
            if (filesystem->name == name) return filesystem->init_fn;
        }

        return nullptr;
    }

    Node* mount(const stl::StringView path) {
        const auto length = check_abs_path(path);
        if (length == 0) return nullptr;

        if (path == "/") {
            if (root != nullptr) return nullptr;

            root = static_cast<Node*>(memory::heap::alloc(sizeof(Node) + length + 1, alignof(Node)));
            init_mount_node(root, nullptr, path.substr(0, length));

            return root;
        }

        Node* parent;
        stl::SplitIterator it;
        auto node = find_node(path, parent, it);

        if (node != nullptr) return nullptr;
        if (parent->type != NodeType::Directory) return nullptr;
        if (it.next()) return nullptr;

        node = parent->children.push_back_alloc(it.entry.size() + 1);
        init_mount_node(node, parent, it.entry);

        return node;
    }

    File* open_file(stl::StringView path, const Mode mode) {
        const auto length = check_abs_path(path);
        if (length == 0) return nullptr;
        path = path.substr(0, length);

        Node* parent;
        stl::SplitIterator it;
        auto node = find_node(path, parent, it);

        if (node == nullptr && !it.next() && is_write(mode) && parent->type == NodeType::Directory) {
            node = parent->fs_ops->create(parent->fs_handle, parent, NodeType::File, it.entry);
        }

        if (node != nullptr) {
            if (node->type != NodeType::File) return nullptr;
            if (node->open_write > 0) return nullptr;
            if (is_write(mode) && node->open_read > 0) return nullptr;

            const auto ops = node->fs_ops->open(node->fs_handle, node, mode);
            if (ops == nullptr) return nullptr;

            if (is_read(mode)) node->open_read++;
            if (is_write(mode)) node->open_write++;

            const auto file = memory::heap::alloc<File>();
            file->ops = ops;
            file->node = node;
            file->mode = mode;
            file->cursor = 0;

            return file;
        }

        return nullptr;
    }

    void close_file(File* file) {
        if (is_read(file->mode)) file->node->open_read--;
        if (is_write(file->mode)) file->node->open_write--;

        file->node->fs_ops->on_close(file->node->fs_handle, file);
        memory::heap::free(file);
    }

    struct Dir {
        Node* node;
        stl::LinkedList<Node>::Iterator it;
    };

    void* open_dir(stl::StringView path) {
        const auto length = check_abs_path(path);
        if (length == 0) return nullptr;
        path = path.substr(0, length);

        Node* parent;
        stl::SplitIterator it;
        const auto node = find_node(path, parent, it);

        if (node != nullptr && node->type == NodeType::Directory) {
            if (!node->populated) node->fs_ops->populate(node->fs_handle, node);

            node->open_read++;

            const auto dir = memory::heap::alloc<Dir>();
            dir->node = node;
            dir->it = node->children.begin();

            return dir;
        }

        return nullptr;
    }

    stl::StringView read_dir(void* dir) {
        const auto d = static_cast<Dir*>(dir);

        if (d->it != stl::LinkedList<Node>::end()) {
            return (d->it++)->name;
        }

        return { "", 0 };
    }

    void close_dir(void* dir) {
        const auto d = static_cast<Dir*>(dir);
        d->node->open_read--;

        memory::heap::free(d);
    }

    bool create_dir(stl::StringView path) {
        const auto length = check_abs_path(path);
        if (length == 0) return false;
        path = path.substr(0, length);

        Node* parent;
        stl::SplitIterator it;
        auto node = find_node(path, parent, it);

        if (node == nullptr && !it.next() && parent->type == NodeType::Directory) {
            node = parent->fs_ops->create(parent->fs_handle, parent, NodeType::Directory, it.entry);
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
            if (!node->populated) node->fs_ops->populate(node->fs_handle, node);
            if (!node->children.empty()) return false;
        }

        return node->fs_ops->destroy(node->fs_handle, node);
    }
} // namespace cosmos::vfs
