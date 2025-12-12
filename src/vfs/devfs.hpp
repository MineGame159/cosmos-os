#pragma once

#include "stl/ring_buffer.hpp"
#include "types.hpp"

namespace cosmos::vfs::devfs {
    void register_filesystem();

    void register_device(Node* node, stl::StringView name, const FileOps* ops, void* handle);

    // Sequence Device

    struct Sequence;

    struct SequenceOps {
        void (*reset)(Sequence* seq);
        void (*next)(Sequence* seq);
        void (*show)(Sequence* seq);
    };

    struct Sequence {
        const SequenceOps* ops;

        uint64_t index;
        bool eof;

        bool show_overflow;

        stl::RingBuffer<char, 512> buffer;

        void printf(const char* fmt, ...);
    };

    void register_sequence_device(Node* node, stl::StringView name, const SequenceOps* ops);

} // namespace cosmos::vfs::devfs
