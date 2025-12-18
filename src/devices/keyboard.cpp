#include "keyboard.hpp"

#include "scheduler/event.hpp"
#include "stl/fixed_list.hpp"
#include "stl/ring_buffer.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::keyboard {
    static stl::RingBuffer<Event, 32> events = {};
    static stl::FixedList<scheduler::EventHandle, 8, 0> event_handles = {};

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
        event_handles.remove_at(data);
        asm volatile("sti" ::: "memory");
    }

    uint64_t kb_ioctl([[maybe_unused]] vfs::File* file, const uint64_t op, [[maybe_unused]] uint64_t arg) {
        switch (op) {
        case IOCTL_CREATE_EVENT: {
            asm volatile("cli" ::: "memory");

            scheduler::EventHandle* handle;
            size_t handle_index;

            if (!event_handles.try_add(handle, handle_index)) {
                asm volatile("sti" ::: "memory");
                return 0;
            }

            *handle = scheduler::create_event(event_destroy, handle_index);

            asm volatile("sti" ::: "memory");
            return *handle;
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
            for (const auto handle : event_handles) {
                scheduler::signal_event(handle);
            }
        }
    }
} // namespace cosmos::devices::keyboard
