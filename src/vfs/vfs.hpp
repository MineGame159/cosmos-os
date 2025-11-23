#pragma once

#include <cstdint>

namespace cosmos::vfs {
    enum class SeekType : uint8_t {
        Start,
        Current,
        End,
    };

    struct FileOps {
        uint64_t (*seek)(void* handle, SeekType type, int64_t offset);
        uint64_t (*read)(void* handle, void* buffer, uint64_t length);
        uint64_t (*write)(void* handle, const void* buffer, uint64_t length);
        void (*close)(void* handle);
    };

    struct File {
        void* handle;
        const FileOps* ops;
    };

    struct DirOps {
        const char* (*read)(void* handle);
        void (*close)(void* handle);
    };

    struct Directory {
        void* handle;
        const DirOps* ops;
    };

    enum class Mode : uint8_t {
        Read,
        Write,
        ReadWrite,
    };

    struct FsOps {
        File* (*open_file)(void* handle, const char* path, Mode mode);
        Directory* (*open_dir)(void* handle, const char* path);
        bool (*make_dir)(void* handle, const char* path);
        bool (*remove)(void* handle, const char* path);
    };

    struct Fs {
        void* handle;
        const FsOps* ops;
    };

    Fs* mount(const char* path);

    File* open_file(const char* path, Mode mode);
    void close_file(File* file);

    Directory* open_dir(const char* path);
    void close_dir(Directory* dir);

    // Create a directory at the given absolute path.
    // Returns true on success.
    bool make_dir(const char* path);

    // Remove a file or an empty directory at the given absolute path.
    // Returns true on success.
    bool remove(const char* path);
} // namespace cosmos::vfs
