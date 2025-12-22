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

    static stl::LinkedList<Process> processes = {};
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

    void delete_process(const Process* process, stl::LinkedList<Process>::Iterator* it_ptr) {
        for (const auto file : process->fd_table) {
            vfs::close(file);
        }

        memory::heap::free(const_cast<char*>(process->cwd.data()));
        memory::virt::destroy(process->space);
        memory::heap::free(process->kernel_stack);

        if (it_ptr != nullptr) {
            processes.remove_free(*it_ptr);
        } else {
            for (auto it = processes.begin(); it != stl::LinkedList<Process>::end(); ++it) {
                if (*it == process) {
                    processes.remove_free(it);
                    break;
                }
            }
        }
    }

    __attribute__((naked)) void user_entry_stub() {
        asm volatile("iretq");
    }

    ProcessId create_process(const ProcessFn fn, const memory::virt::Space space, const Land land, const stl::StringView cwd) {
        const auto process = processes.push_back_alloc();

        // Set basic fields
        process->fn = fn;
        process->land = land;

        process->state = State::Waiting;
        process->status = 0xFFFFFFFF;

        process->space = space;

        process->event_files = nullptr;
        process->event_count = 0;
        process->event_signalled = false;

        process->fd_table = {};

        // Allocate kernel stack
        process->kernel_stack = memory::heap::alloc(KERNEL_STACK_SIZE, 16);

        if (process->kernel_stack == nullptr) {
            ERROR("Failed to allocate memory for kernel stack");

            processes.remove_free(process);

            return 0;
        }

        process->kernel_stack_rsp = 0;

        // Allocate user stack
        if (land == Land::User) {
            process->user_stack_phys = memory::phys::alloc_pages(USER_STACK_SIZE / 4096ul);

            if (process->user_stack_phys == 0) {
                ERROR("Failed to allocate memory for user stack");

                memory::heap::free(process->kernel_stack);
                processes.remove_free(process);

                return 0;
            }

            constexpr auto virt = (memory::virt::LOWER_HALF_END - USER_STACK_SIZE) / 4096ul;
            const auto phys = process->user_stack_phys / 4096ul;
            constexpr auto flags = memory::virt::Flags::Write | memory::virt::Flags::User;
            const auto status = memory::virt::map_pages(space, virt, phys, USER_STACK_SIZE / 4096ul, flags);

            if (!status) {
                memory::phys::free_pages(phys, USER_STACK_SIZE / 4096ul);
                memory::heap::free(process->kernel_stack);
                processes.remove_free(process);

                return 0;
            }
        } else {
            process->user_stack_phys = 0;
        }

        // Setup kernel stack
        auto stack = reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(process->kernel_stack) + KERNEL_STACK_SIZE);

        if (land == Land::Kernel) {
            *--stack = reinterpret_cast<uint64_t>(fn); // Entry point
            *--stack = 0x202;                          // Flags
        } else {
            *--stack = 24 | 3;                       // GDT - User data
            *--stack = memory::virt::LOWER_HALF_END; // User RSP
            *--stack = 0x202;                        // Flags
            *--stack = 32 | 3;                       // GDT - User code

            *--stack = reinterpret_cast<uint64_t>(fn);              // Entry point
            *--stack = reinterpret_cast<uint64_t>(user_entry_stub); // Entry point stub
            *--stack = 0x202;                                       // Flags
        }

        for (auto i = 0ul; i < 15; i++) {
            *--stack = i;
        }

        process->kernel_stack_rsp = reinterpret_cast<uint64_t>(stack);

        // Set cwd
        process->cwd = stl::StringView("", 0);
        set_cwd(reinterpret_cast<ProcessId>(process), cwd);

        return reinterpret_cast<ProcessId>(process);
    }

    ProcessId create_process(const ProcessFn fn, const Land land, const stl::StringView cwd) {
        const auto space = memory::virt::create();
        if (space == 0) return 0;

        const auto id = create_process(fn, space, land, cwd);
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
        const auto process = reinterpret_cast<Process*>(id);

        // Load ELF binary
        if (!elf::load(process->space, file, binary)) {
            delete_process(process, nullptr);
            memory::heap::free(binary);
            vfs::close(file);
            return 0;
        }

        // Cleanup
        memory::heap::free(binary);
        vfs::close(file);

        return id;
    }

    ProcessId get_current_process() {
        return reinterpret_cast<ProcessId>(*current);
    }

    State get_process_state(const ProcessId id) {
        return reinterpret_cast<Process*>(id)->state;
    }

    stl::StringView get_cwd(const ProcessId id) {
        return reinterpret_cast<Process*>(id)->cwd;
    }

    void set_cwd(const ProcessId id, const stl::StringView path) {
        if (path.empty()) return;

        const auto process = reinterpret_cast<Process*>(id);
        const auto old_cwd = process->cwd;

        const auto cwd = memory::heap::alloc_array<char>(path.size() + 1);
        process->cwd = stl::StringView(cwd, path.size());

        utils::memcpy(cwd, path.data(), path.size());
        cwd[path.size()] = '\0';

        if (!old_cwd.empty()) memory::heap::free(const_cast<char*>(old_cwd.data()));
    }

    uint32_t add_fd(const ProcessId id, vfs::File* file) {
        const auto process = reinterpret_cast<Process*>(id);

        const auto index = process->fd_table.add(file);

        if (index == -1) {
            ERROR("Failed to add file descriptor, table full for process %llu", id);
            return 0xFFFFFFFF;
        }

        return index;
    }

    vfs::File* get_file(const ProcessId id, const uint32_t fd) {
        const auto process = reinterpret_cast<Process*>(id);

        return process->fd_table.get(fd);
    }

    vfs::File* remove_fd(const ProcessId id, const uint32_t fd) {
        const auto process = reinterpret_cast<Process*>(id);

        const auto file = process->fd_table.remove_at(fd);
        if (file == nullptr) ERROR("Failed to remove file descriptor %lu from process %llu", fd, id);

        return file;
    }

    void move_next() {
        ++current;

        if (current == stl::LinkedList<Process>::end()) {
            current = processes.begin();
        }
    }

    void switch_to_process(uint64_t* old_rsp, Process* process) {
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
                INFO("Process %llu exited with status %llu", reinterpret_cast<ProcessId>(*current), current->status);

                if (processes.single_item()) {
                    utils::panic(nullptr, "[scheduler] All processes exited, stopping");
                }

                if (*current != old_process) {
                    delete_process(*current, &current);
                    continue;
                }
            }

            if (current->state == State::Waiting || (current->state == State::SuspendedEvents && current->event_signalled)) {
                break;
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
        const auto process = reinterpret_cast<Process*>(id);

        if (process->state == State::Suspended) {
            process->state = State::Waiting;
        }
    }

    void run() {
        asm volatile("cli" ::: "memory");

        utils::msr_write(utils::MSR_KERNEL_GS_BASE, reinterpret_cast<uint64_t>(&cpu_status));

        current = processes.begin();
        uint64_t old;

        switch_to_process(&old, *current);
    }
} // namespace cosmos::scheduler
