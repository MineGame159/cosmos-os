#pragma once

#include "stl/string_view.hpp"
#include "vfs.hpp"

#include <cstdint>

namespace cosmos::vfs {
    struct Fs;

    // File

    enum class SeekType : uint8_t {
        Start,
        Current,
        End,
    };

    struct File;

    struct FileOps {
        uint64_t (*seek)(File* file, SeekType type, int64_t offset);
        uint64_t (*read)(File* file, void* buffer, uint64_t length);
        uint64_t (*write)(File* file, const void* buffer, uint64_t length);
    };

    enum class Mode : uint8_t {
        Read,
        Write,
        ReadWrite,
    };

    inline bool is_read(const Mode mode) {
        return mode == Mode::Read || mode == Mode::ReadWrite;
    }

    inline bool is_write(const Mode mode) {
        return mode == Mode::Write || mode == Mode::ReadWrite;
    }

    struct File {
        Fs* fs;
        const FileOps* ops;
        void* handle;

        Mode mode;
        uint64_t cursor;

        void seek(const uint64_t data_size, const SeekType type, const int64_t offset) {
            switch (type) {
            case SeekType::Start:
                cursor = offset;
                break;
            case SeekType::Current:
                cursor += offset;
                break;
            case SeekType::End:
                cursor = data_size + offset;
                break;
            }
        }
    };

    // Directory

    struct Directory;

    struct DirOps {
        stl::StringView (*read)(Directory* dir);
    };

    struct Directory {
        Fs* fs;
        const DirOps* ops;
        void* handle;

        uint64_t cursor;
    };

    // Filesystem

    struct FsOps {
        File* (*open_file)(Fs* fs, stl::StringView path, Mode mode);
        void (*close_file)(Fs* fs, File* file);

        Directory* (*open_dir)(Fs* fs, stl::StringView path);
        void (*close_dir)(Fs* fs, Directory* dir);

        bool (*make_dir)(Fs* fs, stl::StringView path);
        bool (*remove)(Fs* fs, stl::StringView path);
    };

    struct Fs {
        const FsOps* ops;
        void* handle;
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
