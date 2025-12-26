#include "scheduler.hpp"

#include "elf/loader.hpp"
#include "log/log.hpp"
#include "stl/linked_list.hpp"
#include "tss.hpp"
#include "utils.hpp"

namespace cosmos::task {
    struct CpuStatus {
        uint64_t kernel_rsp;
        uint64_t user_rsp;
        uint64_t current_process;
    };

    static stl::LinkedList<ProcessId> process_queue = {};
    static stl::LinkedList<ProcessId>::Iterator it = {};

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

    static stl::Rc<Process> move_next() {
        ++it;

        if (it == stl::LinkedList<ProcessId>::end()) {
            it = process_queue.begin();
        }

        return get_current_process();
    }

    static void switch_to_process(uint64_t* old_rsp, Process* process) {
        process->state = State::Running;

        cpu_status.kernel_rsp = reinterpret_cast<uint64_t>(process->kernel_stack) + KERNEL_STACK_SIZE;
        cpu_status.current_process = reinterpret_cast<uint64_t>(process);

        tss::set_rsp(0, cpu_status.kernel_rsp);

        memory::virt::switch_to(process->space);
        switch_to(old_rsp, process->kernel_stack_rsp);
    }

    bool enqueue(const ProcessId pid) {
        const auto process = get_process(pid);
        if (!process.valid()) return false;

        *process_queue.push_back_alloc() = process.ref()->id;
        return true;
    }

    bool dequeue(const ProcessId pid) {
        const auto process = get_process(pid);
        if (!process.valid()) return false;

        if (process_queue.remove(pid)) {
            process.deref();
            return true;
        }

        return false;
    }

    stl::Rc<Process> get_current_process() {
        return get_process(**it);
    }

    stl::Optional<uint64_t> join(const ProcessId pid) {
        const auto process = get_process(pid);
        if (!process.valid()) return {};

        const auto current = get_current_process();
        if (process == current) return {};

        current->state = State::SuspendedEvents;
        current->joining_with = *process;

        yield();
        current->joining_with = nullptr;

        const auto status = process->status;

        return status;
    }

    void yield() {
        Process* new_process_ptr = nullptr;
        Process* old_process_ptr = nullptr;

        {
            auto current = get_current_process();

            if (current->state == State::Running) {
                current->state = State::Waiting;
            }

            const auto old_process = current;

            asm volatile("cli" ::: "memory");

            current = move_next();

            for (;;) {
                if (current->state == State::Exited) {
                    DEBUG("Process %llu exited with status %llu", current->id, current->status);

                    if (process_queue.single_item()) {
                        utils::panic(nullptr, "[scheduler] All processes exited, stopping");
                    }

                    const auto next = it.node->next;
                    dequeue(current->id);
                    it.node = next;

                    if (it == process_queue.end()) {
                        it = process_queue.begin();
                    }

                    current = get_current_process();

                    continue;
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

                if (current == old_process) {
                    asm volatile("sti; hlt; cli" ::: "memory");
                }

                current = move_next();
            }

            if (old_process == current) {
                asm volatile("sti" ::: "memory");
                return;
            }

            new_process_ptr = *current;
            old_process_ptr = *old_process;
        }

        switch_to_process(&old_process_ptr->kernel_stack_rsp, new_process_ptr);
        asm volatile("sti" ::: "memory");
    }

    void exit(const uint64_t status) {
        {
            const auto current = get_current_process();

            current->state = State::Exited;
            current->status = status;
        }

        yield();
    }

    void suspend() {
        const auto current = get_current_process();

        current->state = State::Suspended;

        yield();
    }

    void resume(const ProcessId pid) {
        const auto process = get_process(pid);

        if (process.valid() && process->state == State::Suspended) {
            process->state = State::Waiting;
        }
    }

    void run() {
        asm volatile("cli" ::: "memory");
        Process* process_ptr;

        {
            utils::msr_write(utils::MSR_GS_BASE, reinterpret_cast<uint64_t>(&cpu_status));
            utils::msr_write(utils::MSR_KERNEL_GS_BASE, 0);

            it = process_queue.begin();
            process_ptr = *get_current_process();
        }

        uint64_t old;
        switch_to_process(&old, process_ptr);
    }
} // namespace cosmos::task
