#include "vfs.hpp"

#include "memory/heap.hpp"
#include "path.hpp"
#include "stl/linked_list.hpp"
#include "utils.hpp"

namespace cosmos::vfs {
    struct Mount {
        stl::StringView path;
        Fs fs;
    };

    static stl::LinkedList<Mount> mounts = {};

    Fs* mount(const char* path) {
        const auto path_length = check_abs_path(path);
        if (path_length == 0) return nullptr;

        const auto mount = mounts.push_back_alloc(path_length);

        mount->path = stl::StringView(reinterpret_cast<char*>(mount + 1), path_length);
        utils::memcpy(const_cast<char*>(mount->path.data()), path, path_length);

        return &mount->fs;
    }

    const Fs* get_fs(const char* path_str, const char*& fs_path) {
        const auto path = stl::StringView(path_str);

        const auto length = check_abs_path(path);
        if (length == 0) return nullptr;

        uint32_t longest_mount_length = 0;
        const Fs* fs = nullptr;

        for (const auto mount : mounts) {
            if (mount->path.size() == 1 && mount->path[0] == '/' && 1 > longest_mount_length) {
                longest_mount_length = 1;
                fs = &mount->fs;
            } else if (path.starts_with(mount->path)) {
                if ((path[mount->path.size()] == '/' || path.size() == mount->path.size()) && mount->path.size() > longest_mount_length) {
                    longest_mount_length = mount->path.size();
                    fs = &mount->fs;
                }
            }
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
