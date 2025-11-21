#pragma once

#include "memory/virtual.hpp"
#include <cstdint>

namespace cosmos::scheduler {
    using ProcessFn = void (*)();

    enum class State {
        Waiting,
        Running,
        Exited,
    };

    using ProcessId = uint64_t;

    void init();

    ProcessId create_process(ProcessFn fn);
    ProcessId create_process(ProcessFn fn, memory::virt::Space space);

    State get_process_state(ProcessId id);

    void yield();
    void exit();

    void run();
} // namespace cosmos::scheduler
