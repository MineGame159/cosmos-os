#include "pit.hpp"

#include "interrupts/isr.hpp"
#include "utils.hpp"

namespace cosmos::devices::pit {
    struct Repeat {
        uint64_t ms;
        HandlerFn fn;
        uint64_t data;
    };

    constexpr uint16_t CHANNEL0 = 0x40;
    constexpr uint16_t CHANNEL1 = 0x41;
    constexpr uint16_t CHANNEL2 = 0x42;
    constexpr uint16_t COMMAND = 0x43;

    static uint64_t ticks = 0;

    constexpr uint32_t REPEAT_SIZE = 8;
    static Repeat repeats[REPEAT_SIZE];
    static uint32_t repeat_count = 0;

    void tick([[maybe_unused]] isr::InterruptInfo* info) {
        ticks++;

        for (auto i = 0u; i < repeat_count; i++) {
            const auto& repeat = repeats[i];

            if (ticks % repeat.ms == 0) {
                repeat.fn(repeat.data);
            }
        }
    }

    void start() {
        asm volatile("cli" ::: "memory");
        utils::byte_out(COMMAND, 0b00'11'011'0);

        constexpr uint32_t divisor = 1193180u / 1000u;
        utils::byte_out(CHANNEL0, divisor & 0xFF);
        utils::byte_out(CHANNEL0, (divisor >> 8) & 0xFF);

        isr::set(0, tick);
        asm volatile("sti" ::: "memory");
    }

    void run_every_x_ms(const uint64_t ms, const HandlerFn fn, const uint64_t data) {
        if (repeat_count < REPEAT_SIZE) {
            repeats[repeat_count++] = {
                .ms = ms,
                .fn = fn,
                .data = data,
            };
        }
    }
} // namespace cosmos::devices::pit
