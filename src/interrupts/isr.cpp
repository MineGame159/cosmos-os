#include "isr.hpp"

#include "pic.hpp"
#include "serial.hpp"
#include "utils.hpp"

namespace cosmos::isr {
    extern "C" {
    // Exceptions
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

    // Interrupt requests
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

    static handler_fn handlers[16];

    void init() {
        utils::memset(handlers, 0, 16 * sizeof(handler_fn));

        pic::init();

        // Exceptions
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

        // Interrupt requests
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

    void set(const uint8_t num, const handler_fn handler) {
        handlers[num] = handler;
    }

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

    extern "C" void isr_handler(InterruptInfo* info) {
        // Exceptions (32)
        if (info->interrupt < 32) {
            serial::print("[cosmos] KERNEL PANIC\n");
            serial::printf("[cosmos]     %s\n", EXCEPTIONS[info->interrupt]);
            utils::halt();
        }

        // Interrupt requests (16)
        if (info->interrupt < 48) {
            const auto irq = info->interrupt - 32;

            const auto handler = handlers[irq];
            if (handler) {
                handler(info);
            }

            pic::end_irq(irq);
        }
    }
} // namespace cosmos::isr
