#pragma once

#include "scheduler.hpp"
#include "stl/fixed_list.hpp"

namespace cosmos::scheduler {
    struct Event;

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

        stl::FixedList<vfs::File*, 64, nullptr> fd_table;
    };

    struct Event {
        void (*destroy_fn)(uint64_t data);
        uint64_t destroy_data;

        bool signalled;
        Process* waiting_process;
    };
} // namespace cosmos::scheduler
