#pragma once

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

    constexpr uint32_t SEQUENCE_BUFFER_CAPACITY = 512;

    struct Sequence {
        const SequenceOps* ops;

        int64_t index;
        bool show_overflow;

        char buffer[SEQUENCE_BUFFER_CAPACITY];
        uint64_t size;
        uint64_t offset;

        [[nodiscard]]
        uint64_t remaining() const {
            return SEQUENCE_BUFFER_CAPACITY - size;
        }

        void printf(const char* fmt, ...);
    };

    void register_sequence_device(Node* node, stl::StringView name, const SequenceOps* ops);

} // namespace cosmos::vfs::devfs
