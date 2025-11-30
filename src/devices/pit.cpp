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

    constexpr uint32_t REPEAT_CAPACITY = 8;
    static Repeat repeats[REPEAT_CAPACITY] = {};
    static uint32_t repeat_count = 0;

    void tick([[maybe_unused]] isr::InterruptInfo* info) {
        ticks++;

        for (auto i = 0u; i < repeat_count; i++) {
            const auto& repeat = repeats[i];

            if (repeat.ms != 0 && ticks % repeat.ms == 0) {
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

    bool run_every_x_ms(const uint64_t ms, const HandlerFn fn, const uint64_t data) {
        asm volatile("cli" ::: "memory");

        for (auto i = 0u; i < REPEAT_CAPACITY; i++) {
            if (repeats[i].ms == 0) {
                repeats[i] = {
                    .ms = ms,
                    .fn = fn,
                    .data = data,
                };

                if (i == repeat_count) {
                    repeat_count = i + 1;
                }

                asm volatile("sti" ::: "memory");
                return true;
            }
        }

        asm volatile("sti" ::: "memory");
        return false;
    }

    void timer_destroy(const uint64_t data) {
        asm volatile("cli" ::: "memory");

        repeats[data].ms = 0;

        if (data == repeat_count - 1) {
            for (auto i = static_cast<int32_t>(repeat_count) - 1; i >= 0; i--) {
                if (repeats[i].ms != 0) break;
                repeat_count--;
            }
        }

        asm volatile("sti" ::: "memory");
    }

    void timer_tick(const uint64_t event) {
        scheduler::signal_event(event);
    }

    scheduler::EventHandle create_timer(const uint64_t ms) {
        auto found = false;
        auto index = 0u;

        for (auto i = 0u; i < REPEAT_CAPACITY; i++) {
            if (repeats[i].ms == 0) {
                found = true;
                index = i;
                break;
            }
        }

        if (!found) return 0;

        const auto event = scheduler::create_event(timer_destroy, index);
        run_every_x_ms(ms, timer_tick, event);

        return event;
    }
} // namespace cosmos::devices::pit
