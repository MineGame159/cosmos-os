#include "isr.hpp"

#include "pic.hpp"
#include "serial.hpp"
#include "utils.hpp"

namespace cosmos::isr {
    extern "C" {
    // Exceptions 0..31
    void isr0();
    void isr1();
    void isr2();
    void isr3();
    void isr4();
    void isr5();
    void isr6();
    void isr7();
    void isr8();
    void isr9();
    void isr10();
    void isr11();
    void isr12();
    void isr13();
    void isr14();
    void isr15();
    void isr16();
    void isr17();
    void isr18();
    void isr19();
    void isr20();
    void isr21();
    void isr22();
    void isr23();
    void isr24();
    void isr25();
    void isr26();
    void isr27();
    void isr28();
    void isr29();
    void isr30();
    void isr31();

    // IRQs 32..47
    void isr32();
    void isr33();
    void isr34();
    void isr35();
    void isr36();
    void isr37();
    void isr38();
    void isr39();
    void isr40();
    void isr41();
    void isr42();
    void isr43();
    void isr44();
    void isr45();
    void isr46();
    void isr47();
    }

    /// Handlers for IRQs 0..15
    static handler_fn handlers[16];

    /// Naked common ISR routine. RSP points to saved r15 (top of saved registers).
    extern "C" __attribute__((naked)) void isr_common() {
        asm volatile(R"(
        # Preserve base pointer and set new frame
        push %rbp
        mov %rsp, %rbp

        # Save general-purpose registers (caller-saved + callee-saved)
        push %rax
        push %rbx
        push %rcx
        push %rdx
        push %rsi
        push %rdi
        push %r8
        push %r9
        push %r10
        push %r11
        push %r12
        push %r13
        push %r14
        push %r15

        # At this point stack layout (top -> lower):
        # r15, r14, ..., rax, rbp, <interrupt number>, <error code>, <iret frame...>

        # Pass pointer to the top of this saved-register block as first argument (RDI)
        mov %rsp, %rdi

        # Call C++ handler: void isr_handler(InterruptInfo* info)
        call isr_handler

        # Restore registers in reverse order
        pop %r15
        pop %r14
        pop %r13
        pop %r12
        pop %r11
        pop %r10
        pop %r9
        pop %r8
        pop %rdi
        pop %rsi
        pop %rdx
        pop %rcx
        pop %rbx
        pop %rax

        # Restore base pointer
        pop %rbp

        # Remove the two 8-byte values pushed by stubs: interrupt + error
        add $16, %rsp

        # Return from interrupt (pops RIP, CS, RFLAGS [, RSP, SS if present])
        iretq
    )");
    }

/// ISR_NO_ERROR_CODE(n): push 0 (error), push n (interrupt number), jmp isr_common
#define ISR_NO_ERROR_CODE(num)                                                                                                             \
    extern "C" __attribute__((naked)) void isr##num() {                                                                                    \
        asm volatile("cli\n\t"                                                                                                             \
                     "pushq $0\n\t"                                                                                                        \
                     "pushq $" #num "\n\t"                                                                                                 \
                     "jmp isr_common\n");                                                                                                  \
    }

/// ISR_ERROR_CODE(n): push n (error), jmp isr_common
#define ISR_ERROR_CODE(num)                                                                                                                \
    extern "C" __attribute__((naked)) void isr##num() {                                                                                    \
        asm volatile("cli\n\t"                                                                                                             \
                     "pushq $" #num "\n\t"                                                                                                 \
                     "jmp isr_common\n");                                                                                                  \
    }

    // Generate all exception stubs (0..31)
    ISR_NO_ERROR_CODE(0)
    ISR_NO_ERROR_CODE(1)
    ISR_NO_ERROR_CODE(2)
    ISR_NO_ERROR_CODE(3)
    ISR_NO_ERROR_CODE(4)
    ISR_NO_ERROR_CODE(5)
    ISR_NO_ERROR_CODE(6)
    ISR_NO_ERROR_CODE(7)
    ISR_ERROR_CODE(8)
    ISR_NO_ERROR_CODE(9)
    ISR_ERROR_CODE(10)
    ISR_ERROR_CODE(11)
    ISR_ERROR_CODE(12)
    ISR_ERROR_CODE(13)
    ISR_ERROR_CODE(14)
    ISR_NO_ERROR_CODE(15)
    ISR_NO_ERROR_CODE(16)
    ISR_NO_ERROR_CODE(17)
    ISR_NO_ERROR_CODE(18)
    ISR_NO_ERROR_CODE(19)
    ISR_NO_ERROR_CODE(20)
    ISR_NO_ERROR_CODE(21)
    ISR_NO_ERROR_CODE(22)
    ISR_NO_ERROR_CODE(23)
    ISR_NO_ERROR_CODE(24)
    ISR_NO_ERROR_CODE(25)
    ISR_NO_ERROR_CODE(26)
    ISR_NO_ERROR_CODE(27)
    ISR_NO_ERROR_CODE(28)
    ISR_NO_ERROR_CODE(29)
    ISR_NO_ERROR_CODE(30)
    ISR_NO_ERROR_CODE(31)

    // Generate IRQ stubs (32..47)
    ISR_NO_ERROR_CODE(32)
    ISR_NO_ERROR_CODE(33)
    ISR_NO_ERROR_CODE(34)
    ISR_NO_ERROR_CODE(35)
    ISR_NO_ERROR_CODE(36)
    ISR_NO_ERROR_CODE(37)
    ISR_NO_ERROR_CODE(38)
    ISR_NO_ERROR_CODE(39)
    ISR_NO_ERROR_CODE(40)
    ISR_NO_ERROR_CODE(41)
    ISR_NO_ERROR_CODE(42)
    ISR_NO_ERROR_CODE(43)
    ISR_NO_ERROR_CODE(44)
    ISR_NO_ERROR_CODE(45)
    ISR_NO_ERROR_CODE(46)
    ISR_NO_ERROR_CODE(47)

#undef ISR_NO_ERROR_CODE
#undef ISR_ERROR_CODE

    /// Initialize ISR handling: clear handler table, program PIC entries, enable PIC
    void init() {
        // zero handlers
        utils::memset(handlers, 0, sizeof(handlers));

        pic::init();

        // Exceptions (0..31)
        pic::set(0, reinterpret_cast<uint64_t>(isr0), 0x8E);
        pic::set(1, reinterpret_cast<uint64_t>(isr1), 0x8E);
        pic::set(2, reinterpret_cast<uint64_t>(isr2), 0x8E);
        pic::set(3, reinterpret_cast<uint64_t>(isr3), 0x8E);
        pic::set(4, reinterpret_cast<uint64_t>(isr4), 0x8E);
        pic::set(5, reinterpret_cast<uint64_t>(isr5), 0x8E);
        pic::set(6, reinterpret_cast<uint64_t>(isr6), 0x8E);
        pic::set(7, reinterpret_cast<uint64_t>(isr7), 0x8E);
        pic::set(8, reinterpret_cast<uint64_t>(isr8), 0x8E);
        pic::set(9, reinterpret_cast<uint64_t>(isr9), 0x8E);
        pic::set(10, reinterpret_cast<uint64_t>(isr10), 0x8E);
        pic::set(11, reinterpret_cast<uint64_t>(isr11), 0x8E);
        pic::set(12, reinterpret_cast<uint64_t>(isr12), 0x8E);
        pic::set(13, reinterpret_cast<uint64_t>(isr13), 0x8E);
        pic::set(14, reinterpret_cast<uint64_t>(isr14), 0x8E);
        pic::set(15, reinterpret_cast<uint64_t>(isr15), 0x8E);
        pic::set(16, reinterpret_cast<uint64_t>(isr16), 0x8E);
        pic::set(17, reinterpret_cast<uint64_t>(isr17), 0x8E);
        pic::set(18, reinterpret_cast<uint64_t>(isr18), 0x8E);
        pic::set(19, reinterpret_cast<uint64_t>(isr19), 0x8E);
        pic::set(20, reinterpret_cast<uint64_t>(isr20), 0x8E);
        pic::set(21, reinterpret_cast<uint64_t>(isr21), 0x8E);
        pic::set(22, reinterpret_cast<uint64_t>(isr22), 0x8E);
        pic::set(23, reinterpret_cast<uint64_t>(isr23), 0x8E);
        pic::set(24, reinterpret_cast<uint64_t>(isr24), 0x8E);
        pic::set(25, reinterpret_cast<uint64_t>(isr25), 0x8E);
        pic::set(26, reinterpret_cast<uint64_t>(isr26), 0x8E);
        pic::set(27, reinterpret_cast<uint64_t>(isr27), 0x8E);
        pic::set(28, reinterpret_cast<uint64_t>(isr28), 0x8E);
        pic::set(29, reinterpret_cast<uint64_t>(isr29), 0x8E);
        pic::set(30, reinterpret_cast<uint64_t>(isr30), 0x8E);
        pic::set(31, reinterpret_cast<uint64_t>(isr31), 0x8E);

        // IRQs (32..47)
        pic::set(32, reinterpret_cast<uint64_t>(isr32), 0x8E);
        pic::set(33, reinterpret_cast<uint64_t>(isr33), 0x8E);
        pic::set(34, reinterpret_cast<uint64_t>(isr34), 0x8E);
        pic::set(35, reinterpret_cast<uint64_t>(isr35), 0x8E);
        pic::set(36, reinterpret_cast<uint64_t>(isr36), 0x8E);
        pic::set(37, reinterpret_cast<uint64_t>(isr37), 0x8E);
        pic::set(38, reinterpret_cast<uint64_t>(isr38), 0x8E);
        pic::set(39, reinterpret_cast<uint64_t>(isr39), 0x8E);
        pic::set(40, reinterpret_cast<uint64_t>(isr40), 0x8E);
        pic::set(41, reinterpret_cast<uint64_t>(isr41), 0x8E);
        pic::set(42, reinterpret_cast<uint64_t>(isr42), 0x8E);
        pic::set(43, reinterpret_cast<uint64_t>(isr43), 0x8E);
        pic::set(44, reinterpret_cast<uint64_t>(isr44), 0x8E);
        pic::set(45, reinterpret_cast<uint64_t>(isr45), 0x8E);
        pic::set(46, reinterpret_cast<uint64_t>(isr46), 0x8E);
        pic::set(47, reinterpret_cast<uint64_t>(isr47), 0x8E);

        pic::update();

        serial::print("[isr] Initialized\n");
    }

    /// Register an IRQ handler (0..15)
    void set(const uint8_t num, const handler_fn handler) {
        if (num < 16) {
            handlers[num] = handler;
        }
    }

    /// Exception descriptions
    constexpr const char* EXCEPTIONS[] = {
        "Division By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Into Detected Overflow",
        "Out of Bounds",
        "Invalid Opcode",
        "No Coprocessor",

        "Double Fault",
        "Coprocessor Segment Overrun",
        "Bad TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault",
        "Unknown Interrupt",

        "Coprocessor Fault",
        "Alignment Check",
        "Machine Check",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",

        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
    };

    /// C++ interrupt handler called from isr_common.
    ///
    /// info->interrupt is the interrupt number (0..47 etc.)
    extern "C" void isr_handler(InterruptInfo* info) {
        if (!info) {
            return;
        }

        // Exceptions (0..31) -> panic
        if (info->interrupt < 32) {
            serial::print("[cosmos] KERNEL PANIC\n");
            const char* name = "Unknown";
            if (info->interrupt < (sizeof(EXCEPTIONS) / sizeof(EXCEPTIONS[0]))) {
                name = EXCEPTIONS[info->interrupt];
            }
            serial::printf("[cosmos]     %s\n", name);
            utils::halt();
        }

        // IRQs (32..47)
        if (info->interrupt < 48) {
            const auto irq = static_cast<uint8_t>(info->interrupt - 32);
            const auto handler = handlers[irq];
            if (handler) {
                handler(info);
            }
            pic::end_irq(irq);
        }
    }
} // namespace cosmos::isr
