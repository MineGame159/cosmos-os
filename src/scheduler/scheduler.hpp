#pragma once

#include "memory/virtual.hpp"
#include "stl/string_view.hpp"

#include <cstdint>

namespace cosmos::scheduler {
    using ProcessFn = uint32_t (*)();

    enum class State : uint8_t {
        Waiting,
        Running,
        Suspended,
        SuspendedEvents,
        Exited,
    };

    using ProcessId = uint64_t;

    ProcessId create_process(ProcessFn fn);
    ProcessId create_process(ProcessFn fn, memory::virt::Space space);
    ProcessId create_process(stl::StringView path);

    ProcessId get_current_process();
    State get_process_state(ProcessId id);

    void yield();
    void exit(uint32_t status);

    void suspend();
    void resume(ProcessId id);

    void run();
} // namespace cosmos::scheduler
