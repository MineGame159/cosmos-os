#pragma once

#include "stl/linked_list.hpp"
#include "stl/string_view.hpp"

#include <cstdint>

namespace cosmos::vfs {
    enum class NodeType : uint8_t;
    struct Node;
    struct FileOps;
    enum class Mode : uint8_t;
    struct File;

    // Fs

    struct FsOps {
        Node* (*create)(void* handle, Node* parent, NodeType type, stl::StringView name);
        bool (*destroy)(void* handle, Node* node);

        void (*populate)(void* handle, Node* node);
        const FileOps* (*open)(void* handle, const Node* node, Mode mode);
        void (*on_close)(void* handle, const File* file);
    };

    // Node

    enum class NodeType : uint8_t {
        Directory,
        File,
    };

    struct Node {
        Node* parent;

        NodeType type;
        stl::StringView name;

        const FsOps* fs_ops;
        void* fs_handle;

        uint16_t open_read;
        uint16_t open_write;

        bool populated;
        stl::LinkedList<Node> children;
    };

    // File

    enum class SeekType : uint8_t {
        Start,
        Current,
        End,
    };

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
        const FileOps* ops;
        Node* node;

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
} // namespace cosmos::vfs
