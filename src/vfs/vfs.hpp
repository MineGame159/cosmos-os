#pragma once

#include "stl/string_view.hpp"

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
        const stl::StringView& (*read)(void* handle);
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
        File* (*open_file)(void* handle, stl::StringView path, Mode mode);
        Directory* (*open_dir)(void* handle, stl::StringView path);
        bool (*make_dir)(void* handle, stl::StringView path);
        bool (*remove)(void* handle, stl::StringView path);
    };

    struct Fs {
        void* handle;
        const FsOps* ops;
    };

    Fs* mount(stl::StringView path);

    File* open_file(stl::StringView path, Mode mode);
    void close_file(File* file);

    Directory* open_dir(stl::StringView path);
    void close_dir(Directory* dir);

    /// Create a directory at the given absolute path.
    /// Returns true on success.
    bool make_dir(stl::StringView path);

    /// Remove a file or an empty directory at the given absolute path.
    /// Returns true on success.
    bool remove(stl::StringView path);
} // namespace cosmos::vfs
