#include "devfs.hpp"

#include "nanoprintf.h"
#include "utils.hpp"
#include "vfs.hpp"

#include <cstdarg>

namespace cosmos::vfs::devfs {
    // FsOps

    Node* fs_create([[maybe_unused]] Node* parent, [[maybe_unused]] NodeType type, [[maybe_unused]] stl::StringView name) {
        return nullptr;
    }

    bool fs_destroy([[maybe_unused]] Node* node) {
        return false;
    }

    void fs_populate(Node* node) {
        node->populated = true;
    }

    const FileOps* fs_open(const Node* node, const Mode mode) {
        const auto ops = *reinterpret_cast<FileOps* const*>(node + 1);

        if (is_read(mode) && ops->read == nullptr) return nullptr;
        if (is_write(mode) && ops->write == nullptr) return nullptr;

        return ops;
    }

    void fs_on_close([[maybe_unused]] const File* file) {}

    static constexpr FsOps fs_ops = {
        .create = fs_create,
        .destroy = fs_destroy,
        .populate = fs_populate,
        .open = fs_open,
        .on_close = fs_on_close,
    };

    // Basic

    bool init(Node* node, [[maybe_unused]] stl::StringView device_path) {
        node->fs_ops = &fs_ops;
        node->fs_handle = nullptr;

        node->populated = true;

        return true;
    }

    void register_filesystem() {
        vfs::register_filesystem("devfs", 0, init);
    }

    Node* alloc_node(Node* node, const stl::StringView name, const FileOps* ops, void* handle, const uint64_t additional_size) {
        const auto device = node->children.push_back_alloc(sizeof(FileOps*) + additional_size + name.size() + 1);
        utils::memset(device, 0, sizeof(Node));
        *reinterpret_cast<const FileOps**>(device + 1) = ops;

        device->parent = node;
        device->type = NodeType::File;
        device->name = stl::StringView(reinterpret_cast<char*>(device) + sizeof(Node) + sizeof(FileOps*) + additional_size, name.size());
        device->fs_ops = &fs_ops;
        device->fs_handle = handle;

        utils::memcpy(const_cast<char*>(device->name.data()), name.data(), name.size());
        const_cast<char*>(device->name.data())[name.size()] = '\0';

        return device;
    }

    void register_device(Node* node, stl::StringView name, const FileOps* ops, void* handle) {
        if (name.contains("/")) return;
        name = name.trim();

        alloc_node(node, name, ops, handle, 0);
    }

    // Sequence Device

    uint64_t sequence_seek(File* file, [[maybe_unused]] SeekType type, [[maybe_unused]] int64_t offset) {
        if (type == SeekType::Current && offset == 0) {
            return file->cursor;
        }

        const auto seq = reinterpret_cast<Sequence*>(reinterpret_cast<uint8_t*>(file->node) + sizeof(Node) + sizeof(FileOps*));
        seq->ops->reset(seq);

        seq->size = 0;
        seq->offset = 0;

        file->cursor = 0;
        return 0;
    }

    uint64_t sequence_read(File* file, void* buffer, const uint64_t length) {
        const auto seq = reinterpret_cast<Sequence*>(reinterpret_cast<uint8_t*>(file->node) + sizeof(Node) + sizeof(FileOps*));

        // Generate data into buffer
        while (seq->remaining() >= 64 && !seq->eof) {
            const auto prev_size = seq->size;
            const auto prev_offset = seq->offset;

            seq->show_overflow = false;
            seq->ops->show(seq);

            if (seq->show_overflow) {
                seq->size = prev_size;
                seq->offset = prev_offset;
                break;
            }

            seq->ops->next(seq);
        }

        // Read data from buffer
        const auto read = utils::min(seq->size - seq->offset, length);
        if (read == 0) return 0;

        utils::memcpy(buffer, &seq->buffer[seq->offset], read);
        seq->offset += read;

        // Move rest of the buffer to the start
        utils::memcpy(seq->buffer, &seq->buffer[seq->offset], seq->size - seq->offset);
        seq->size -= read;
        seq->offset = 0;

        file->cursor += read;
        return read;
    }

    uint64_t sequence_ioctl([[maybe_unused]] File* file, [[maybe_unused]] uint64_t op, [[maybe_unused]] uint64_t arg) {
        return IOCTL_UNKNOWN;
    }

    static constexpr FileOps sequence_file_ops = {
        .seek = sequence_seek,
        .read = sequence_read,
        .write = nullptr,
        .ioctl = sequence_ioctl,
    };

    void Sequence::printf(const char* fmt, ...) {
        static char fmt_buffer[128];

        if (show_overflow) return;

        va_list args;
        va_start(args, fmt);
        const auto length = static_cast<uint64_t>(npf_vsnprintf(fmt_buffer, 128, fmt, args));
        va_end(args);

        if (length > remaining()) {
            show_overflow = true;
            return;
        }

        utils::memcpy(&buffer[size], fmt_buffer, length);
        size += length;
    }

    void register_sequence_device(Node* node, stl::StringView name, const SequenceOps* ops) {
        if (name.contains("/")) return;
        name = name.trim();

        const auto device = alloc_node(node, name, &sequence_file_ops, nullptr, sizeof(Sequence));
        const auto seq = reinterpret_cast<Sequence*>(reinterpret_cast<uint8_t*>(device) + sizeof(Node) + sizeof(FileOps*));

        seq->ops = ops;
        seq->index = 0;
        seq->show_overflow = false;
        seq->size = 0;
        seq->offset = 0;

        seq->ops->reset(seq);
    }
} // namespace cosmos::vfs::devfs
