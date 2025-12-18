#include "init.hpp"

#include "utils.hpp"

namespace cosmos::syscalls {
    __attribute__((naked)) void entry() {
        asm volatile(R"(
            # Switch to Kernel GS to access per-cpu data
            swapgs

            # Save the User Stack Pointer (RSP) to a scratch slot
            mov %rsp, %gs:8

            # Load Kernel RSP from offset 0
            mov %gs:0, %rsp

            # Save User context required for sysretq
            pushq %gs:8
            pushq %r11
            pushq %rcx

            # Save General Purpose Registers
            pushq %rax
            pushq %rbx
            pushq %rcx
            pushq %rdx
            pushq %rsi
            pushq %rdi
            pushq %rbp
            pushq %r8
            pushq %r9
            pushq %r10
            pushq %r11
            pushq %r12
            pushq %r13
            pushq %r14
            pushq %r15

            # Prepare C++ Function Call
            mov %r10, %rcx  # Restore Arg4 to RCX for C++ (since syscall used R10)

            # Pass the stack pointer to C++
            mov %rax, %rdi  # 1st Arg: Syscall Number
            mov %rsp, %rsi  # 2nd Arg: Pointer to registers (Context)

            call syscall_handler

            # Return Logic (For syscalls that return, e.g. NOT exit)
            pop %r15
            pop %r14
            pop %r13
            pop %r12
            pop %r11 # Pop RFLAGS junk
            pop %r10
            pop %r9
            pop %r8
            pop %rbp
            pop %rdi
            pop %rsi
            pop %rdx
            pop %rcx # Pop RIP junk
            pop %rbx
            pop %rax # Restore Return Value (or result from C++)

            # Restore Hardware Context
            pop %rcx     # Restore User RIP
            pop %r11     # Restore User RFLAGS
            pop %rsp     # Restore User RSP (Dangerous! We switch stacks here)

            # NOTE: The `pop rsp` above works because we are about to execute `swapgs`
            # and `sysretq` immediately. Interrupts are disabled (FMASK).

            swapgs
            sysretq
        )");
    }

    void init() {
        // Enable SCE (System Call Extensions)
        const auto efer = utils::msr_read(utils::MSR_EFER);
        utils::msr_write(utils::MSR_EFER, efer | 1);

        // Set address of syscall entry
        utils::msr_write(utils::MSR_LSTAR, reinterpret_cast<uint64_t>(entry));

        // Set segments
        constexpr uint64_t kernel_code = 8;
        constexpr uint64_t user_base = 16;
        utils::msr_write(utils::MSR_STAR, (user_base << 48) | (kernel_code << 32));

        // Set RFLAGS (interrupts disabled)
        utils::msr_write(utils::MSR_SFMASK, 0x200);
    }
} // namespace cosmos::syscalls
