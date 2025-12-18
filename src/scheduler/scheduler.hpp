#pragma once

#include "memory/virtual.hpp"
#include "stl/string_view.hpp"

#include <cstdint>

namespace cosmos::scheduler {
    using ProcessFn = void (*)();

    enum class State : uint8_t {
        Waiting,
        Running,
        Suspended,
        SuspendedEvents,
        Exited,
    };

    enum class Land : uint8_t {
        Kernel,
        User,
    };

    using ProcessId = uint64_t;

    ProcessId create_process(ProcessFn fn, memory::virt::Space space, Land land);
    ProcessId create_process(ProcessFn fn, Land land);
    ProcessId create_process(stl::StringView path);

    ProcessId get_current_process();
    State get_process_state(ProcessId id);

    void yield();
    void exit(uint64_t status);

    void suspend();
    void resume(ProcessId id);

    void run();
} // namespace cosmos::scheduler
