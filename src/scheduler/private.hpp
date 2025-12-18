#pragma once

#include "scheduler.hpp"

namespace cosmos::scheduler {
    struct Event;

    constexpr uint32_t FD_TABLE_SIZE = 64;

    struct Process {
        ProcessFn fn;
        Land land;

        State state;
        uint64_t status;

        memory::virt::Space space;

        void* kernel_stack;
        uint64_t kernel_stack_rsp;

        uint64_t user_stack_phys;

        Event** events;
        uint32_t event_count;
        bool event_signalled;

        vfs::File* fd_table[FD_TABLE_SIZE];
    };

    struct Event {
        void (*destroy_fn)(uint64_t data);
        uint64_t destroy_data;

        bool signalled;
        Process* waiting_process;
    };
} // namespace cosmos::scheduler
