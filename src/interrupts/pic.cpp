#include "pic.hpp"

#include "utils.hpp"

namespace cosmos::pic {
    constexpr uint16_t MASTER_COMMAND = 0x20;
    constexpr uint16_t MASTER_DATA = 0x21;

    constexpr uint16_t SLAVE_COMMAND = 0xA0;
    constexpr uint16_t SLAVE_DATA = 0xA1;

    struct IdtEntry {
        uint16_t offset_1;
        uint16_t selector;
        uint8_t ist;
        uint8_t flags;
        uint16_t offset_2;
        uint32_t offset_3;
        uint32_t reserved;
    } __attribute__((packed));

    struct IdtTablePtr {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    static IdtEntry entries[256];
    static IdtTablePtr ptr;

    void init() {
        utils::memset(entries, 0, 256 * sizeof(IdtEntry));

        ptr.limit = 256 * sizeof(IdtEntry) - 1;
        ptr.base = reinterpret_cast<uint64_t>(entries);

        // Start initialization sequence
        utils::byte_out(MASTER_COMMAND, 0x11);
        utils::wait();
        utils::byte_out(SLAVE_COMMAND, 0x11);
        utils::wait();

        // Send offsets
        utils::byte_out(MASTER_DATA, 0x20);
        utils::wait();
        utils::byte_out(SLAVE_DATA, 0x28);
        utils::wait();

        // Send master - slave relationship
        utils::byte_out(MASTER_DATA, 0x04);
        utils::wait();
        utils::byte_out(SLAVE_DATA, 0x02);
        utils::wait();

        // Send mode (8086)
        utils::byte_out(MASTER_DATA, 0x01);
        utils::wait();
        utils::byte_out(SLAVE_DATA, 0x01);
        utils::wait();

        // Send masks (0 - all IRQs enabled)
        utils::byte_out(MASTER_DATA, 0x00);
        utils::wait();
        utils::byte_out(SLAVE_DATA, 0x00);
        utils::wait();
    }

    void set(const uint8_t num, const uint64_t handler, const uint8_t flags) {
        entries[num] = {
            .offset_1 = static_cast<uint16_t>(handler),
            .selector = 40, // GDT - 64-bit code descriptor
            .ist = 0,
            .flags = flags,
            .offset_2 = static_cast<uint16_t>(handler >> 16),
            .offset_3 = static_cast<uint32_t>(handler >> 32),
            .reserved = 0,
        };
    }

    void update() {
        asm volatile("lidt %0" ::"m"(ptr) : "memory");
        asm volatile("sti");
    }

    void end_irq(const uint8_t number) {
        if (number >= 8)
            utils::byte_out(SLAVE_COMMAND, 0x20);

        utils::byte_out(MASTER_COMMAND, 0x20);
    }
} // namespace cosmos::pic
