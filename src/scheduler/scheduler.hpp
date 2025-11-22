#pragma once

#include "memory/virtual.hpp"

#include <cstdint>

namespace cosmos::scheduler {
    using ProcessFn = void (*)();

    enum class State : uint8_t {
        Waiting,
        Running,
        Suspended,
        Exited,
    };

    using ProcessId = uint64_t;

    void init();

    ProcessId create_process(ProcessFn fn);
    ProcessId create_process(ProcessFn fn, memory::virt::Space space);

    ProcessId get_current_process();
    State get_process_state(ProcessId id);

    void yield();
    void exit();

    void suspend();
    void resume(ProcessId id);

    void run();
} // namespace cosmos::scheduler
