#include "pit.hpp"

#include "interrupts/isr.hpp"
#include "stl/fixed_list.hpp"
#include "task/event.hpp"
#include "task/scheduler.hpp"
#include "utils.hpp"
#include "vfs/devfs.hpp"

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

    static void tick([[maybe_unused]] isr::InterruptInfo* info) {
        ticks++;

        for (const auto& repeat : repeats) {
            if (repeat.ms != 0 && ticks % repeat.ms == 0) {
                repeat.fn(repeat.data);
            }
        }
    }

    // VFS

    static uint64_t seek([[maybe_unused]] const stl::Rc<vfs::File>& file, [[maybe_unused]] vfs::SeekType type,
                         [[maybe_unused]] int64_t offset) {
        return 0;
    }

    static uint64_t read([[maybe_unused]] const stl::Rc<vfs::File>& file, void* buffer, const uint64_t length) {
        if (length != sizeof(uint64_t)) return 0;

        *static_cast<uint64_t*>(buffer) = ticks;
        return sizeof(uint64_t);
    }

    static void event_close(const uint64_t index) {
        asm volatile("cli" ::: "memory");

        repeats.remove_at(index);

        asm volatile("sti" ::: "memory");
    }

    static void event_tick(const uint64_t event_file_ptr) {
        const auto event_file = reinterpret_cast<vfs::File*>(event_file_ptr);

        constexpr uint64_t number = 1;
        event_file->ops->write(event_file, &number, sizeof(uint64_t));
    }

    static uint64_t ioctl([[maybe_unused]] const stl::Rc<vfs::File>& file, const uint64_t op, const uint64_t arg) {
        switch (op) {
        case IOCTL_CREATE_EVENT: {
            const auto index = repeats.index_of({});
            if (index == -1) return 0;

            uint32_t fd;
            const auto event_file = *task::create_event(event_close, index, vfs::FileFlags::CloseOnExecute, fd);

            if (event_file == nullptr) {
                return fd;
            }

            run_every_x_ms(arg, event_tick, reinterpret_cast<uint64_t>(event_file));

            return fd;
        }

        default:
            return vfs::IOCTL_UNKNOWN;
        }
    }

    static constexpr vfs::FileOps ops = {
        .seek = seek,
        .read = read,
        .write = nullptr,
        .ioctl = ioctl,
    };

    // Header

    void init(vfs::Node* node) {
        asm volatile("cli" ::: "memory");
        utils::byte_out(COMMAND, 0b00'11'011'0);

        constexpr uint32_t divisor = 1193180u / 1000u;
        utils::byte_out(CHANNEL0, divisor & 0xFF);
        utils::byte_out(CHANNEL0, (divisor >> 8) & 0xFF);

        isr::set(0, tick);
        asm volatile("sti" ::: "memory");

        vfs::devfs::register_device(node, "timer", &ops, nullptr);
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
} // namespace cosmos::devices::pit
