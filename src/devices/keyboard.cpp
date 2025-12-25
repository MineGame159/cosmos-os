#include "keyboard.hpp"

#include "stl/fixed_list.hpp"
#include "stl/ring_buffer.hpp"
#include "task/event.hpp"
#include "task/scheduler.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::keyboard {
    static stl::RingBuffer<Event, 32> events = {};
    static stl::FixedList<vfs::File*, 8, nullptr> event_files = {};

    static uint64_t kb_seek([[maybe_unused]] vfs::File* file, [[maybe_unused]] vfs::SeekType type, [[maybe_unused]] int64_t offset) {
        return 0;
    }

    static uint64_t kb_read(vfs::File* file, void* buffer, const uint64_t length) {
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

    static void event_close(const uint64_t index) {
        asm volatile("cli" ::: "memory");
        event_files.remove_at(index);
        asm volatile("sti" ::: "memory");
    }

    static uint64_t kb_ioctl([[maybe_unused]] vfs::File* file, const uint64_t op, [[maybe_unused]] uint64_t arg) {
        switch (op) {
        case IOCTL_CREATE_EVENT: {
            asm volatile("cli" ::: "memory");

            vfs::File** event_file;
            size_t event_file_index;

            if (!event_files.try_add(event_file, event_file_index)) {
                asm volatile("sti" ::: "memory");
                return 0;
            }

            uint32_t fd;
            *event_file = task::create_event(event_close, event_file_index, fd).value();

            if (*event_file == nullptr) {
                event_files.remove_at(event_file_index);
            }

            asm volatile("sti" ::: "memory");
            return fd;
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
            for (const auto event_file : event_files) {
                constexpr uint64_t number = 1;
                event_file->ops->write(event_file, &number, sizeof(uint64_t));
            }
        }
    }
} // namespace cosmos::devices::keyboard
