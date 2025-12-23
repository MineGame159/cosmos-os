#include "scheduler.hpp"

#include "elf/loader.hpp"
#include "elf/parser.hpp"
#include "log/log.hpp"
#include "memory/heap.hpp"
#include "memory/offsets.hpp"
#include "memory/physical.hpp"
#include "private.hpp"
#include "stl/linked_list.hpp"
#include "tss.hpp"
#include "utils.hpp"
#include "vfs/vfs.hpp"

namespace cosmos::scheduler {
    struct CpuStatus {
        uint64_t kernel_rsp;
        uint64_t user_rsp;
        uint64_t current_process;
    };

    constexpr uint64_t KERNEL_STACK_SIZE = 4ul * 1024ul;
    constexpr uint64_t USER_STACK_SIZE = 64ul * 1024ul;

    static stl::FixedList<Process*, 256, nullptr> processes = {};

    static stl::LinkedList<Process> process_queue = {};
    static stl::LinkedList<Process>::Iterator current = {};

    static CpuStatus cpu_status = {};

    __attribute__((naked)) void switch_to(uint64_t* old_sp, uint64_t new_sp) {
        asm volatile(R"(
            # Save current process state to the stack
            pushfq
            push %rax
            push %rbx
            push %rcx
            push %rdx
            push %rsi
            push %rdi
            push %rbp
            push %r8
            push %r9
            push %r10
            push %r11
            push %r12
            push %r13
            push %r14
            push %r15

            # Write the stack pointer of the current process into a pointer (first argument, rdi)
            mov %rsp, (%rdi)

            # Replace the stack pointer of the current process with the new process (second argument, rsi)
            mov %rsi, %rsp

            # Load new process state from the stack
            pop %r15
            pop %r14
            pop %r13
            pop %r12
            pop %r11
            pop %r10
            pop %r9
            pop %r8
            pop %rbp
            pop %rdi
            pop %rsi
            pop %rdx
            pop %rcx
            pop %rbx
            pop %rax
            popfq

            # Return to the code the process was executing previously
            ret
        )");
    }

    Process* get_process(const ProcessId id) {
        return processes.get(id);
    }

    static void unref_process(Process* process) {
        process->ref_count--;
        if (process->ref_count > 0) return;

        for (const auto file : process->fd_table) {
            vfs::close(file);
        }

        memory::heap::free(const_cast<char*>(process->cwd.data()));
        memory::virt::destroy(process->space);
        memory::heap::free(process->kernel_stack);

        processes.remove_at(process->id);

        const auto node = reinterpret_cast<uint8_t*>(process) - offsetof(stl::LinkedList<Process>::Node, item);
        memory::heap::free(node);
    }

    static void remove_process_from_queue(Process* process, stl::LinkedList<Process>::Iterator* it_ptr) {
        if (it_ptr != nullptr) {
            process_queue.remove(*it_ptr);
        } else {
            for (auto it = process_queue.begin(); it != stl::LinkedList<Process>::end(); ++it) {
                if (*it == process) {
                    process_queue.remove(it);
                    break;
                }
            }
        }

        unref_process(process);
    }

    __attribute__((naked)) void user_entry_stub() {
        asm volatile("swapgs; iretq");
    }

    void setup_dummy_frame(StackFrame& frame, const ProcessFn fn) {
        for (auto i = 0ul; i < 15; i++) {
            frame[i] = i;
        }

        frame.rip = reinterpret_cast<uint64_t>(fn);
        frame.rflags = 0x202;
        frame.user_rsp = memory::virt::LOWER_HALF_END;
    }

    ProcessId create_process(const ProcessFn fn, const memory::virt::Space space, const Land land, bool alloc_user_stack,
                             const StackFrame& frame, const stl::StringView cwd) {
        const auto process = process_queue.push_back_alloc();

        // Allocate id
        const auto id = processes.add(process);

        if (id == -1) {
            ERROR("Failed to create process, too many processes");
            process_queue.remove_free(process);
            return 0;
        }

        process->id = id;

        // Set basic fields
        process->ref_count = 1;

        process->fn = fn;
        process->land = land;

        process->state = State::Waiting;
        process->status = 0xFFFFFFFF;

        process->space = space;

        process->joining_with = nullptr;

        process->event_files = nullptr;
        process->event_count = 0;
        process->event_signalled = false;

        process->fd_table = {};

        // Allocate kernel stack
        process->kernel_stack = memory::heap::alloc(KERNEL_STACK_SIZE, 16);

        if (process->kernel_stack == nullptr) {
            ERROR("Failed to allocate memory for kernel stack");

            processes.remove(process);
            process_queue.remove_free(process);

            return 0;
        }

        process->kernel_stack_rsp = 0;

        // Allocate user stack
        if (land == Land::User) {
            constexpr auto virt = (memory::virt::LOWER_HALF_END - USER_STACK_SIZE) / 4096ul;

            if (alloc_user_stack) {
                process->user_stack_phys = memory::phys::alloc_pages(USER_STACK_SIZE / 4096ul);

                if (process->user_stack_phys == 0) {
                    ERROR("Failed to allocate memory for user stack");

                    memory::heap::free(process->kernel_stack);
                    processes.remove(process);
                    process_queue.remove_free(process);

                    return 0;
                }

                const auto phys = process->user_stack_phys / 4096ul;
                constexpr auto flags = memory::virt::Flags::Write | memory::virt::Flags::User;
                const auto status = memory::virt::map_pages(space, virt, phys, USER_STACK_SIZE / 4096ul, flags);

                if (!status) {
                    memory::phys::free_pages(phys, USER_STACK_SIZE / 4096ul);
                    memory::heap::free(process->kernel_stack);
                    processes.remove(process);
                    process_queue.remove_free(process);

                    return 0;
                }
            } else {
                process->user_stack_phys = memory::virt::get_phys(virt);
            }
        } else {
            process->user_stack_phys = 0;
        }

        // Setup kernel stack
        auto stack = reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(process->kernel_stack) + KERNEL_STACK_SIZE);

        if (land == Land::Kernel) {
            *--stack = frame.rip;    // Entry point
            *--stack = frame.rflags; // Flags
        } else {
            *--stack = 24 | 3;         // GDT - User data
            *--stack = frame.user_rsp; // User RSP
            *--stack = frame.rflags;   // Flags
            *--stack = 32 | 3;         // GDT - User code
            *--stack = frame.rip;      // Entry point

            *--stack = reinterpret_cast<uint64_t>(user_entry_stub); // Entry point stub
            *--stack = 0x2;                                         // Flags
        }

        for (auto i = 0ul; i < 15; i++) {
            *--stack = frame[i];
        }

        process->kernel_stack_rsp = reinterpret_cast<uint64_t>(stack);

        // Set cwd
        process->cwd = stl::StringView("", 0);
        set_cwd(process->id, cwd);

        return process->id;
    }

    ProcessId create_process(const ProcessFn fn, const Land land, const stl::StringView cwd) {
        // Create address space
        const auto space = memory::virt::create();
        if (space == 0) return 0;

        // Setup stack frame
        StackFrame frame;
        setup_dummy_frame(frame, fn);

        // Create process
        const auto id = create_process(fn, space, land, true, frame, cwd);
        if (id == 0) memory::virt::destroy(space);

        return id;
    }

    ProcessId create_process(const stl::StringView path, const stl::StringView cwd) {
        // Open file
        const auto file = vfs::open(path, vfs::Mode::Read);

        if (file == nullptr) {
            ERROR("Failed to open file");
            return 0;
        }

        // Parse ELF binary
        const auto binary = elf::parse(file);

        if (binary == nullptr) {
            vfs::close(file);
            return 0;
        }

        // Create process
        const auto id = create_process(reinterpret_cast<ProcessFn>(binary->virt_entry), Land::User, cwd);

        if (id == 0) {
            memory::heap::free(binary);
            vfs::close(file);
            return 0;
        }

        INFO("Creating process %llu for file %s", id, path.data());
        const auto process = processes.get(id);

        // Load ELF binary
        if (!elf::load(process->space, file, binary)) {
            remove_process_from_queue(process, nullptr);
            memory::heap::free(binary);
            vfs::close(file);
            return 0;
        }

        // Cleanup
        memory::heap::free(binary);
        vfs::close(file);

        return id;
    }

    ProcessId fork(const ProcessId other_id, const StackFrame& frame) {
        const auto other = processes.get(other_id);
        if (other == nullptr) return 0;

        if (other->land != Land::User) {
            ERROR("Can only fork user-land processes");
            return 0;
        }

        // Fork address space
        const auto space = memory::virt::fork(other->space);
        if (space == 0) return 0;

        // Create process
        const auto id = create_process(other->fn, space, other->land, false, frame, other->cwd);

        if (id == 0) {
            memory::virt::destroy(space);
            return 0;
        }

        const auto process = processes.get(id);

        // Duplicate file descriptors
        for (auto it = other->fd_table.begin(); it != other->fd_table.end(); ++it) {
            const auto file = vfs::duplicate(*it);
            process->fd_table.set(it.index, file);
        }

        return id;
    }

    ProcessId get_current_process() {
        return current->id;
    }

    State get_process_state(const ProcessId id) {
        const auto process = processes.get(id);
        return process != nullptr ? process->state : State::Exited;
    }

    stl::StringView get_cwd(const ProcessId id) {
        const auto process = processes.get(id);
        return process != nullptr ? process->cwd : "";
    }

    void set_cwd(const ProcessId id, const stl::StringView path) {
        if (path.empty()) return;

        const auto process = processes.get(id);
        if (process == nullptr) return;

        const auto old_cwd = process->cwd;

        const auto cwd = memory::heap::alloc_array<char>(path.size() + 1);
        process->cwd = stl::StringView(cwd, path.size());

        utils::memcpy(cwd, path.data(), path.size());
        cwd[path.size()] = '\0';

        if (!old_cwd.empty()) memory::heap::free(const_cast<char*>(old_cwd.data()));
    }

    uint32_t add_fd(const ProcessId id, vfs::File* file) {
        const auto process = processes.get(id);
        if (process == nullptr) return 0xFFFFFFFF;

        const auto index = process->fd_table.add(file);

        if (index == -1) {
            ERROR("Failed to add file descriptor, table full for process %llu", id);
            return 0xFFFFFFFF;
        }

        return index;
    }

    vfs::File* get_file(const ProcessId id, const uint32_t fd) {
        const auto process = processes.get(id);
        return process != nullptr ? process->fd_table.get(fd) : nullptr;
    }

    vfs::File* remove_fd(const ProcessId id, const uint32_t fd) {
        const auto process = processes.get(id);
        if (process == nullptr) return nullptr;

        const auto file = process->fd_table.remove_at(fd);
        if (file == nullptr) ERROR("Failed to remove file descriptor %lu from process %llu", fd, id);

        return file;
    }

    uint64_t join(const ProcessId id) {
        const auto process = processes.get(id);
        if (process == nullptr || process == *current) return 0xFFFFFFFFFFFFFFFF;

        process->ref_count++;

        current->state = State::SuspendedEvents;
        current->joining_with = process;

        yield();
        current->joining_with = nullptr;

        const auto status = process->status;
        unref_process(process);

        return status;
    }

    static void move_next() {
        ++current;

        if (current == stl::LinkedList<Process>::end()) {
            current = process_queue.begin();
        }
    }

    static void switch_to_process(uint64_t* old_rsp, Process* process) {
        process->state = State::Running;

        cpu_status.kernel_rsp = reinterpret_cast<uint64_t>(process->kernel_stack) + KERNEL_STACK_SIZE;
        cpu_status.current_process = reinterpret_cast<uint64_t>(process);

        tss::set_rsp(0, cpu_status.kernel_rsp);

        memory::virt::switch_to(process->space);
        switch_to(old_rsp, process->kernel_stack_rsp);
    }

    void yield() {
        if (current->state == State::Running) {
            current->state = State::Waiting;
        }

        const auto old_process = *current;

        asm volatile("cli" ::: "memory");

        move_next();

        for (;;) {
            if (current->state == State::Exited) {
                INFO("Process %llu exited with status %llu", current->id, current->status);

                if (process_queue.single_item()) {
                    utils::panic(nullptr, "[scheduler] All processes exited, stopping");
                }

                if (*current != old_process) {
                    remove_process_from_queue(*current, &current);

                    if (current == process_queue.end()) {
                        current = process_queue.begin();
                    }

                    continue;
                }
            }

            if (current->state == State::Waiting) {
                break;
            }

            if (current->state == State::SuspendedEvents) {
                if (current->joining_with != nullptr) {
                    if (current->joining_with->state == State::Exited) break;
                } else {
                    if (current->event_signalled) break;
                }
            }

            if (*current == old_process) {
                asm volatile("sti; hlt; cli" ::: "memory");
            }

            move_next();
        }

        if (old_process != *current) {
            switch_to_process(&old_process->kernel_stack_rsp, *current);
        }

        asm volatile("sti" ::: "memory");
    }

    void exit(const uint64_t status) {
        current->state = State::Exited;
        current->status = status;

        yield();
    }

    void suspend() {
        current->state = State::Suspended;
        yield();
    }

    void resume(const ProcessId id) {
        const auto process = processes.get(id);

        if (process != nullptr && process->state == State::Suspended) {
            process->state = State::Waiting;
        }
    }

    void run() {
        asm volatile("cli" ::: "memory");

        utils::msr_write(utils::MSR_GS_BASE, reinterpret_cast<uint64_t>(&cpu_status));
        utils::msr_write(utils::MSR_KERNEL_GS_BASE, 0);

        current = process_queue.begin();
        uint64_t old;

        switch_to_process(&old, *current);
    }
} // namespace cosmos::scheduler
