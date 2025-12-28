#pragma once

#include "stl/rc.hpp"
#include "stl/ring_buffer.hpp"
#include "vfs/types.hpp"

namespace cosmos::task {
    constexpr uint64_t PIPE_CAPACITY = 64 * 1024;

    struct Pipe {
        uint64_t ref_count;
        uint64_t reader_count;
        uint64_t writer_count;

        stl::RingBuffer<uint8_t, PIPE_CAPACITY> buffer;
    };

    /// Creates a unidirectional pipe with two "ends".
    /// Each "end" will block if there is either no data to be read or if the pipe is full.
    bool create_pipe(vfs::FileFlags flags, stl::Rc<vfs::File>& read_file, stl::Rc<vfs::File>& write_file);
} // namespace cosmos::task
