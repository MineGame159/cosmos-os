#pragma once

#include "memory/virtual.hpp"
#include "stl/string_view.hpp"
#include "vfs/types.hpp"

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

    uint32_t add_fd(ProcessId id, vfs::File* file);
    vfs::File* get_file(ProcessId id, uint32_t fd);
    vfs::File* remove_fd(ProcessId id, uint32_t fd);

    void yield();
    void exit(uint64_t status);

    void suspend();
    void resume(ProcessId id);

    void run();
} // namespace cosmos::scheduler
