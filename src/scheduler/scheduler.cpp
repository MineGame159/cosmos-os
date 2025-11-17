#include "scheduler.hpp"

#include "memory/heap.hpp"
#include "serial.hpp"
#include "utils.hpp"

namespace cosmos::scheduler {
    static Process* head;
    static Process* tail;

    static Process* current;

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

    void process_exit() {
        serial::print("[scheduler] Process exited\n");
        utils::halt();
    }

    void init() {
        head = nullptr;
        tail = nullptr;
        current = nullptr;
    }

    Process* create_process(const ProcessFn fn) {
        const auto process = memory::heap::alloc<Process>();

        if (head == nullptr) {
            head = process;
            tail = process;
        } else {
            tail->next = process;
            tail = process;
        }

        process->fn = fn;
        process->state = State::Waiting;

        process->stack = memory::heap::alloc(STACK_SIZE);
        process->stack_top = reinterpret_cast<void*>((reinterpret_cast<uint64_t>(process->stack) + STACK_SIZE) & ~0xFULL);

        auto stack = static_cast<uint64_t*>(process->stack_top);

        *--stack = reinterpret_cast<uint64_t>(process_exit);
        *--stack = reinterpret_cast<uint64_t>(fn);
        *--stack = 0x202;

        for (auto i = 0ul; i < 15; i++) {
            *--stack = i;
        }

        process->rsp = reinterpret_cast<uint64_t>(stack);

        return process;
    }

    void yield() {
        current->state = State::Waiting;

        const auto prev = current;
        current = current->next;

        if (current == nullptr) current = head;

        current->state = State::Running;
        switch_to(&prev->rsp, current->rsp);
    }

    void run() {
        current = head;
        uint64_t old;

        current->state = State::Running;
        switch_to(&old, current->rsp);
    }
} // namespace cosmos::scheduler
