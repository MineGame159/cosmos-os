#include "serial.hpp"
#include "utils.hpp"

namespace cosmos::serial {
    constexpr uint16_t COM1 = 0x3F8;

    static bool DISABLED = true;

    bool init() {
        utils::byte_out(COM1 + 1, 0x00); // Disable all interrupts
        utils::byte_out(COM1 + 3, 0x80); // Enable DLAB (set baud rate divisor)
        utils::byte_out(COM1 + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
        utils::byte_out(COM1 + 1, 0x00); //                  (hi byte)
        utils::byte_out(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
        utils::byte_out(COM1 + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
        utils::byte_out(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
        utils::byte_out(COM1 + 4, 0x1E); // Set in loopback mode, test the serial chip
        utils::byte_out(COM1 + 0, 0xAE); // Test serial chip (send byte 0xAE and check if serial returns same byte)

        // Check if serial is faulty (i.e: not same byte as sent)
        if (utils::byte_in(COM1 + 0) != 0xAE) {
            return false;
        }

        // If serial is not faulty set it in normal operation mode
        // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
        utils::byte_out(COM1 + 4, 0x0F);

        DISABLED = false;
        return true;
    }

    bool is_transmit_empty() {
        return utils::byte_in(COM1 + 5) & 0x20;
    }

    void wait_for_transmit() {
        while (!is_transmit_empty()) {
            utils::wait();
        }
    }

    void print(const char* str) {
        if (DISABLED)
            return;

        auto i = 0;

        for (;;) {
            const auto ch = str[i++];

            if (ch == '\0')
                break;

            if (ch == '\n') {
                wait_for_transmit();
                utils::byte_out(COM1, '\r');
            }

            wait_for_transmit();
            utils::byte_out(COM1, ch);
        }
    }
} // namespace cosmos::serial
