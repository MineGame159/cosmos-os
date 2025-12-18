#include "pit.hpp"

#include "interrupts/isr.hpp"
#include "stl/fixed_list.hpp"
#include "utils.hpp"

namespace cosmos::devices::pit {
    struct Repeat {
        uint64_t ms;
        HandlerFn fn;
        uint64_t data;
    };

    static bool operator==(const Repeat& lhs, const Repeat& rhs) {
        return lhs.ms == rhs.ms;
    }

    constexpr uint16_t CHANNEL0 = 0x40;
    constexpr uint16_t CHANNEL1 = 0x41;
    constexpr uint16_t CHANNEL2 = 0x42;
    constexpr uint16_t COMMAND = 0x43;

    static uint64_t ticks = 0;

    static stl::FixedList<Repeat, 8, {}> repeats = {};

    void tick([[maybe_unused]] isr::InterruptInfo* info) {
        ticks++;

        for (const auto& repeat : repeats) {
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

        const auto result = repeats.add({
            .ms = ms,
            .fn = fn,
            .data = data,
        });

        asm volatile("sti" ::: "memory");

        return result;
    }

    void timer_destroy(const uint64_t data) {
        asm volatile("cli" ::: "memory");
        repeats.remove_at(data);
        asm volatile("sti" ::: "memory");
    }

    void timer_tick(const uint64_t event) {
        scheduler::signal_event(event);
    }

    scheduler::EventHandle create_timer(const uint64_t ms) {
        const auto index = repeats.index_of({});
        if (index == -1) return 0;

        const auto event = scheduler::create_event(timer_destroy, index);
        run_every_x_ms(ms, timer_tick, event);

        return event;
    }
} // namespace cosmos::devices::pit
