#include "scheduler.hpp"

#include "elf/loader.hpp"
#include "elf/parser.hpp"
#include "log/log.hpp"
#include "memory/heap.hpp"
#include "private.hpp"
#include "stl/linked_list.hpp"
#include "utils.hpp"
#include "vfs/vfs.hpp"

namespace cosmos::scheduler {
    static stl::LinkedList<Process> processes = {};
    static stl::LinkedList<Process>::Iterator current = {};

    constexpr uint64_t STACK_SIZE = 64ul * 1024ul;

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
        memory::virt::destroy(process->space);
        memory::heap::free(process->stack);

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

    __attribute__((naked)) void return_trampoline() {
        asm volatile(R"(
            mov %%rax, %%rdi
            jmp %P0
        )" ::"i"(exit));
    }

    ProcessId create_process(const ProcessFn fn) {
        const auto space = memory::virt::create();
        if (space == 0) return 0;

        return create_process(fn, space);
    }

    ProcessId create_process(const ProcessFn fn, const memory::virt::Space space) {
        const auto process = processes.push_back_alloc();

        process->fn = fn;
        process->state = State::Waiting;

        process->space = space;

        process->stack = memory::heap::alloc(STACK_SIZE, 16);
        process->stack_top = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(process->stack) + STACK_SIZE);

        auto stack = static_cast<uint64_t*>(process->stack_top);

        *--stack = reinterpret_cast<uint64_t>(return_trampoline);
        *--stack = reinterpret_cast<uint64_t>(fn);
        *--stack = 0x202;

        for (auto i = 0ul; i < 15; i++) {
            *--stack = i;
        }

        process->rsp = reinterpret_cast<uint64_t>(stack);

        return reinterpret_cast<ProcessId>(process);
    }

    ProcessId create_process(const stl::StringView path) {
        // Open file
        const auto file = vfs::open_file(path, vfs::Mode::Read);

        if (file == nullptr) {
            ERROR("Failed to open file");
            return 0;
        }

        // Parse ELF binary
        const auto binary = elf::parse(file);

        if (binary == nullptr) {
            vfs::close_file(file);
            return 0;
        }

        // Create process
        const auto id = create_process(reinterpret_cast<ProcessFn>(binary->virt_entry));

        if (id == 0) {
            memory::heap::free(binary);
            vfs::close_file(file);
            return 0;
        }

        INFO("Creating process %llu for file %s", id, path.data());
        const auto process = reinterpret_cast<Process*>(id);

        // Load ELF binary
        const auto prev_space = memory::virt::get_current();
        memory::virt::switch_to(process->space);

        if (!elf::load(file, binary)) {
            memory::virt::switch_to(prev_space);
            delete_process(process, nullptr);
            memory::heap::free(binary);
            vfs::close_file(file);
            return 0;
        }

        memory::virt::switch_to(prev_space);

        // Cleanup
        memory::heap::free(binary);
        vfs::close_file(file);

        return id;
    }

    ProcessId get_current_process() {
        return reinterpret_cast<ProcessId>(*current);
    }

    State get_process_state(const ProcessId id) {
        return reinterpret_cast<Process*>(id)->state;
    }

    void move_next() {
        ++current;

        if (current == stl::LinkedList<Process>::end()) {
            current = processes.begin();
        }
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
                INFO("Process %llu exited with status %lu", reinterpret_cast<ProcessId>(*current), current->status);

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

        current->state = State::Running;

        if (old_process != *current) {
            memory::virt::switch_to(current->space);
            switch_to(&old_process->rsp, current->rsp);
        }

        asm volatile("sti" ::: "memory");
    }

    void exit(const uint32_t status) {
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

        current = processes.begin();
        uint64_t old;

        current->state = State::Running;

        memory::virt::switch_to(current->space);
        switch_to(&old, current->rsp);
    }
} // namespace cosmos::scheduler
