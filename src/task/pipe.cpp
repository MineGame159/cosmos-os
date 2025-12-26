#include "pipe.hpp"

#include "memory/heap.hpp"
#include "scheduler.hpp"

namespace cosmos::task {
    // File operations

    static uint64_t pipe_seek([[maybe_unused]] vfs::File* file, [[maybe_unused]] vfs::SeekType type, [[maybe_unused]] int64_t offset) {
        return 0;
    }

    static uint64_t pipe_read(vfs::File* file, void* buffer, const uint64_t length) {
        if (!vfs::is_read(file->mode)) return 0;

        const auto pipe = *reinterpret_cast<Pipe**>(file + 1);

        while (pipe->buffer.size() == 0) {
            uint64_t count;
            __atomic_load(&pipe->writer_count, &count, __ATOMIC_ACQUIRE);
            if (count == 0) return 0;

            yield();
        }

        return pipe->buffer.try_get(static_cast<uint8_t*>(buffer), length);
    }

    static uint64_t pipe_write(vfs::File* file, const void* buffer, uint64_t length) {
        if (!vfs::is_write(file->mode)) return 0;

        const auto pipe = *reinterpret_cast<Pipe**>(file + 1);

        auto bytes = static_cast<const uint8_t*>(buffer);
        uint64_t written = 0;

        while (length > 0) {
            while (pipe->buffer.remaining() == 0) {
                uint64_t count;
                __atomic_load(&pipe->reader_count, &count, __ATOMIC_ACQUIRE);
                if (count == 0) return written;

                yield();
            }

            const auto write = stl::min(length, pipe->buffer.remaining());
            pipe->buffer.add(bytes, write);

            bytes += write;
            length -= write;
            written += write;
        }

        return written;
    }

    static uint64_t pipe_ioctl([[maybe_unused]] vfs::File* file, [[maybe_unused]] uint64_t op, [[maybe_unused]] uint64_t arg) {
        return vfs::IOCTL_UNKNOWN;
    }

    static constexpr vfs::FileOps pipe_ops = {
        .seek = pipe_seek,
        .read = pipe_read,
        .write = pipe_write,
        .ioctl = pipe_ioctl,
    };

    // File callbacks

    static void pipe_close(vfs::File* file) {
        const auto pipe = *reinterpret_cast<Pipe**>(file + 1);

        if (vfs::is_read(file->mode)) __atomic_sub_fetch(&pipe->reader_count, 1, __ATOMIC_RELEASE);
        if (vfs::is_write(file->mode)) __atomic_sub_fetch(&pipe->writer_count, 1, __ATOMIC_RELEASE);

        if (__atomic_sub_fetch(&pipe->ref_count, 1, __ATOMIC_ACQ_REL) == 0) {
            memory::heap::free(pipe);
        }
    }

    static void pipe_duplicate(vfs::File* file) {
        const auto pipe = *reinterpret_cast<Pipe**>(file + 1);

        if (vfs::is_read(file->mode)) __atomic_add_fetch(&pipe->reader_count, 1, __ATOMIC_RELAXED);
        if (vfs::is_write(file->mode)) __atomic_add_fetch(&pipe->writer_count, 1, __ATOMIC_RELAXED);

        __atomic_add_fetch(&pipe->ref_count, 1, __ATOMIC_RELAXED);
    }

    // Header

    bool create_pipe(vfs::File*& read_file, vfs::File*& write_file) {
        // Allocate pipe
        const auto pipe = memory::heap::alloc<Pipe>();
        if (pipe == nullptr) return false;

        // Allocate read file
        read_file = memory::heap::alloc<vfs::File>(sizeof(Pipe*));

        if (read_file == nullptr) {
            memory::heap::free(pipe);
            return false;
        }

        // Allocate write file
        write_file = memory::heap::alloc<vfs::File>(sizeof(Pipe*));

        if (write_file == nullptr) {
            memory::heap::free(read_file);
            memory::heap::free(pipe);
            return false;
        }

        // Fill pipe
        pipe->ref_count = 2;
        pipe->reader_count = 1;
        pipe->writer_count = 1;
        pipe->buffer.reset();

        // Fill read file
        read_file->ops = &pipe_ops;
        read_file->on_close = pipe_close;
        read_file->on_duplicate = pipe_duplicate;
        read_file->node = nullptr;
        read_file->ref_count = 1;
        read_file->mode = vfs::Mode::Read;
        read_file->cursor = 0;

        *reinterpret_cast<Pipe**>(read_file + 1) = pipe;

        // Fill write file
        write_file->ops = &pipe_ops;
        write_file->on_close = pipe_close;
        write_file->on_duplicate = pipe_duplicate;
        write_file->node = nullptr;
        write_file->ref_count = 1;
        write_file->mode = vfs::Mode::Write;
        write_file->cursor = 0;

        *reinterpret_cast<Pipe**>(write_file + 1) = pipe;

        return true;
    }
} // namespace cosmos::task
