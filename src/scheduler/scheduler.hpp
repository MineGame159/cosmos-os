#pragma once

#include <cstdint>

namespace cosmos::scheduler {
    using ProcessFn = void (*)();

    enum class State {
        Waiting,
        Running,
    };

    struct Process {
        Process* next;

        ProcessFn fn;
        State state;

        void* stack;
        void* stack_top;
        uint64_t rsp;
    };

    void init();

    Process* create_process(ProcessFn fn);

    void yield();

    void run();
} // namespace cosmos::scheduler
