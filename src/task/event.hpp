#pragma once

#include "process.hpp"

namespace cosmos::task {
    struct Event {
        void (*close_fn)(uint64_t data);
        uint64_t close_data;

        uint64_t number;
        Process* waiting_process;
    };

    /// Returns nullptr on failure and fd is set to 0xFFFFFFFF
    stl::PtrOptional<vfs::File*> create_event(void (*close_fn)(uint64_t data), uint64_t close_data, uint32_t& fd);

    uint64_t wait_on_events(vfs::File** event_files, uint32_t count, bool reset_signalled);
} // namespace cosmos::task
