#include "keyboard.hpp"

#include "scheduler/event.hpp"
#include "stl/ring_buffer.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::keyboard {
    static stl::RingBuffer<Event, 32> events = {};

    constexpr uint32_t EVENT_HANDLE_CAPACITY = 8;
    static scheduler::EventHandle event_handles[EVENT_HANDLE_CAPACITY];
    static uint32_t event_handle_count = 0;

    uint64_t kb_seek([[maybe_unused]] vfs::File* file, [[maybe_unused]] vfs::SeekType type, [[maybe_unused]] int64_t offset) {
        return 0;
    }

    uint64_t kb_read(vfs::File* file, void* buffer, const uint64_t length) {
        if (length != sizeof(Event)) return 0;

        asm volatile("cli" ::: "memory");

        Event event;
        if (events.try_get(event)) {
            *static_cast<Event*>(buffer) = event;

            asm volatile("sti" ::: "memory");
            return sizeof(Event);
        }

        asm volatile("sti" ::: "memory");
        return 0;
    }

    void event_destroy(const uint64_t data) {
        asm volatile("cli" ::: "memory");
        event_handles[data] = 0;

        if (data == event_handle_count - 1) {
            for (auto i = static_cast<int32_t>(event_handle_count) - 1; i >= 0; i--) {
                if (event_handles[i] != 0) break;
                event_handle_count--;
            }
        }

        asm volatile("sti" ::: "memory");
    }

    uint64_t kb_ioctl([[maybe_unused]] vfs::File* file, const uint64_t op, [[maybe_unused]] uint64_t arg) {
        switch (op) {
        case IOCTL_CREATE_EVENT: {
            asm volatile("cli" ::: "memory");

            auto found = false;
            auto index = 0u;

            for (auto i = 0u; i < EVENT_HANDLE_CAPACITY; i++) {
                if (event_handles[i] == 0) {
                    found = true;
                    index = i;
                    break;
                }
            }

            if (!found) {
                asm volatile("sti" ::: "memory");
                return 0;
            }

            const auto event = scheduler::create_event(event_destroy, index);
            event_handles[index] = event;

            if (index >= event_handle_count) {
                event_handle_count = index + 1;
            }

            asm volatile("sti" ::: "memory");
            return event;
        }
        case IOCTL_RESET_BUFFER: {
            asm volatile("cli" ::: "memory");

            events.reset();

            asm volatile("sti" ::: "memory");
            return vfs::IOCTL_OK;
        }
        default: {
            return vfs::IOCTL_UNKNOWN;
        }
        }
    }

    static constexpr vfs::FileOps file_ops = {
        .seek = kb_seek,
        .read = kb_read,
        .write = nullptr,
        .ioctl = kb_ioctl,
    };

    void init(vfs::Node* node) {
        vfs::devfs::register_device(node, "keyboard", &file_ops, nullptr);
    }

    void add_event(const Event event) {
        if (events.add(event)) {
            for (auto i = 0u; i < event_handle_count; i++) {
                if (event_handles[i] != 0) scheduler::signal_event(event_handles[i]);
            }
        }
    }
} // namespace cosmos::devices::keyboard
