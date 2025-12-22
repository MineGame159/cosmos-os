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

    struct StackFrame {
        uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
        uint64_t rip, rflags, user_rsp;

        const uint64_t& operator[](const uint32_t index) const {
            return (&r15)[14 - (index < 15 ? index : 14)];
        }
        uint64_t& operator[](const uint32_t index) {
            return (&r15)[14 - (index < 15 ? index : 14)];
        }
    };

    using ProcessId = uint64_t;

    void setup_dummy_frame(StackFrame& frame, ProcessFn fn);

    ProcessId create_process(ProcessFn fn, memory::virt::Space space, Land land, const StackFrame& frame, stl::StringView cwd);
    ProcessId create_process(ProcessFn fn, Land land, stl::StringView cwd);
    ProcessId create_process(stl::StringView path, stl::StringView cwd);

    ProcessId fork(ProcessId other_id, const StackFrame& frame);

    ProcessId get_current_process();
    State get_process_state(ProcessId id);

    stl::StringView get_cwd(ProcessId id);
    void set_cwd(ProcessId id, stl::StringView path);

    /// Returns 0xFFFFFFFF on failure
    uint32_t add_fd(ProcessId id, vfs::File* file);
    vfs::File* get_file(ProcessId id, uint32_t fd);
    vfs::File* remove_fd(ProcessId id, uint32_t fd);

    void yield();
    void exit(uint64_t status);

    void suspend();
    void resume(ProcessId id);

    void run();
} // namespace cosmos::scheduler
