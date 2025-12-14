#pragma once

#include "scheduler.hpp"

namespace cosmos::scheduler {
    struct Event;

    struct Process {
        ProcessFn fn;
        State state;
        uint32_t status;

        memory::virt::Space space;

        void* stack;
        void* stack_top;
        uint64_t rsp;

        Event** events;
        uint32_t event_count;
        bool event_signalled;
    };

    struct Event {
        void (*destroy_fn)(uint64_t data);
        uint64_t destroy_data;

        bool signalled;
        Process* waiting_process;
    };
} // namespace cosmos::scheduler
