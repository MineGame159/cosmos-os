#pragma once

#include "memory/virtual.hpp"
#include "stl/fixed_list.hpp"
#include "stl/optional.hpp"
#include "stl/rc.hpp"
#include "vfs/types.hpp"

namespace cosmos::task {
    constexpr uint64_t KERNEL_STACK_SIZE = 4ul * 1024ul;
    constexpr uint64_t USER_STACK_SIZE = 64ul * 1024ul;

    using ProcessFn = void (*)();
    using ProcessId = uint32_t;

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

    struct Process {
        ProcessId id;
        size_t ref_count;

        Land land;

        State state;
        uint64_t status;

        memory::virt::Space space;

        void* kernel_stack;
        uint64_t kernel_stack_rsp;

        uint64_t user_stack_phys;

        Process* joining_with;

        const stl::Rc<vfs::File>* event_files;
        uint32_t event_count;
        bool event_signalled;

        stl::StringView cwd;

        stl::FixedList<vfs::File*, 64, nullptr> fd_table;

        // Methods

        bool set_cwd(stl::StringView path);

        stl::Optional<uint32_t> add_fd(const stl::Rc<vfs::File>& file);
        bool set_fd(const stl::Rc<vfs::File>& file, uint32_t fd);
        stl::Rc<vfs::File> get_file(uint32_t fd) const;
        stl::Rc<vfs::File> remove_fd(uint32_t fd);

        stl::Optional<ProcessId> fork(const StackFrame& frame) const;

        /// Returns the virtual address of the entry point if it successfully loaded the executable
        stl::Optional<uint64_t> execute(stl::StringView path);

        void destroy();
    };

    void setup_dummy_frame(StackFrame& frame, ProcessFn fn);

    [[noreturn]]
    void reaper_process();

    stl::Optional<ProcessId> create_process(memory::virt::Space space, Land land, bool alloc_user_stack, const StackFrame& frame,
                                            stl::StringView cwd);
    stl::Optional<ProcessId> create_process(ProcessFn fn, Land land, stl::StringView cwd);
    stl::Optional<ProcessId> create_process(stl::StringView path, stl::StringView cwd);

    stl::Rc<Process> get_process(ProcessId id);
} // namespace cosmos::task
