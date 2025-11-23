#include "vfs.hpp"

#include "memory/heap.hpp"
#include "path.hpp"
#include "utils.hpp"

namespace cosmos::vfs {
    struct Mount {
        Mount* next;

        char* path;
        uint32_t path_length;

        Fs fs;
    };

    static Mount* head = nullptr;
    static Mount* tail = nullptr;

    Fs* mount(const char* path) {
        const auto path_length = check_abs_path(path);
        if (path_length == 0) return nullptr;

        const auto mount = static_cast<Mount*>(memory::heap::alloc(sizeof(Mount) + path_length + 1));
        mount->next = nullptr;

        if (head == nullptr) {
            head = mount;
            tail = mount;
        } else {
            tail->next = mount;
            tail = mount;
        }

        mount->path = reinterpret_cast<char*>(mount + 1);
        mount->path_length = path_length;

        utils::memcpy(mount->path, path, path_length);
        mount->path[path_length] = '\0';

        return &mount->fs;
    }

    const Fs* get_fs(const char* path, const char*& fs_path) {
        const auto length = check_abs_path(path);
        if (length == 0) return nullptr;

        auto mount = head;

        uint32_t longest_mount_length = 0;
        const Fs* fs = nullptr;

        while (mount != nullptr) {
            if (mount->path_length == 1 && mount->path[0] == '/' && 1 > longest_mount_length) {
                longest_mount_length = 1;
                fs = &mount->fs;
            } else if (utils::str_has_prefix(path, mount->path)) {
                if ((path[mount->path_length] == '/' || path[mount->path_length] == '\0') && mount->path_length > longest_mount_length) {
                    longest_mount_length = mount->path_length;
                    fs = &mount->fs;
                }
            }

            mount = mount->next;
        }

        if (fs != nullptr) {
            const char* subpath;

            if (longest_mount_length == 1) {
                subpath = &path[0];
            } else if (path[longest_mount_length] == '\0') {
                subpath = "/";
            } else {
                subpath = &path[longest_mount_length];
            }

            fs_path = subpath;
            return fs;
        }

        return nullptr;
    }

    File* open_file(const char* path, const Mode mode) {
        const char* fs_path;
        const auto fs = get_fs(path, fs_path);
        if (fs == nullptr) return nullptr;

        return fs->ops->open_file(fs->handle, fs_path, mode);
    }

    void close_file(File* file) {
        file->ops->close(file->handle);
        memory::heap::free(file);
    }

    Directory* open_dir(const char* path) {
        const char* fs_path;
        const auto fs = get_fs(path, fs_path);
        if (fs == nullptr) return nullptr;

        return fs->ops->open_dir(fs->handle, fs_path);
    }

    void close_dir(Directory* dir) {
        dir->ops->close(dir->handle);
        memory::heap::free(dir);
    }

    bool make_dir(const char* path) {
        const char* fs_path;
        const auto fs = get_fs(path, fs_path);
        if (fs == nullptr) return false;

        if (fs->ops->make_dir == nullptr) return false;
        return fs->ops->make_dir(fs->handle, fs_path);
    }

    bool remove(const char* path) {
        const char* fs_path;
        const auto fs = get_fs(path, fs_path);
        if (fs == nullptr) return false;

        if (fs->ops->remove == nullptr) return false;
        return fs->ops->remove(fs->handle, fs_path);
    }
} // namespace cosmos::vfs
