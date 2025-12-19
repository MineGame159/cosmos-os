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

        vfs::File** event_files;
        uint32_t event_count;
        bool event_signalled;

        stl::FixedList<vfs::File*, 64, nullptr> fd_table;
    };

    struct Event {
        void (*close_fn)(uint64_t data);
        uint64_t close_data;

        uint64_t number;
        Process* waiting_process;
    };
} // namespace cosmos::scheduler
