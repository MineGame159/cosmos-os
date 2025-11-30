#include "keyboard.hpp"

#include "scheduler/scheduler.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::keyboard {
    constexpr uint32_t BUFFER_SIZE = 32;
    static Event event_buffer[BUFFER_SIZE];
    static uint32_t event_buffer_write_index = 0;
    static uint32_t event_buffer_read_index = 0;

    static scheduler::ProcessId waiting_process = 0;

    uint64_t kb_seek([[maybe_unused]] vfs::File* file, [[maybe_unused]] vfs::SeekType type, [[maybe_unused]] int64_t offset) {
        return 0;
    }

    uint64_t kb_read(vfs::File* file, void* buffer, const uint64_t length) {
        if (length != sizeof(Event)) return 0;

        asm volatile("cli" ::: "memory");

        if (event_buffer_read_index != event_buffer_write_index) {
            *static_cast<Event*>(buffer) = event_buffer[event_buffer_read_index];

            event_buffer_read_index = (event_buffer_read_index + 1) % BUFFER_SIZE;

            asm volatile("sti" ::: "memory");
            return sizeof(Event);
        }

        asm volatile("sti" ::: "memory");
        return 0;
    }

    uint64_t kb_ioctl([[maybe_unused]] vfs::File* file, const uint64_t op, [[maybe_unused]] uint64_t arg) {
        switch (op) {
        case IOCTL_RESET_BUFFER:
            asm volatile("cli");

            event_buffer_write_index = 0;
            event_buffer_read_index = 0;

            asm volatile("sti");
            return vfs::IOCTL_OK;

        case IOCTL_RESUME_ON_EVENT:
            waiting_process = scheduler::get_current_process();
            return vfs::IOCTL_OK;

        default:
            return vfs::IOCTL_UNKNOWN;
        }
    }

    static constexpr vfs::FileOps file_ops = {
        .seek = kb_seek,
        .read = kb_read,
        .write = nullptr,
        .ioctl = kb_ioctl,
    };

    void init(vfs::Node* node) {
        vfs::devfs::register_device(node, "keyboard", &file_ops);
    }

    void add_event(const Event event) {
        const auto next_write_index = (event_buffer_write_index + 1) % BUFFER_SIZE;

        if (next_write_index != event_buffer_read_index) {
            event_buffer[event_buffer_write_index] = event;
            event_buffer_write_index = next_write_index;

            // Resume waiting process
            if (waiting_process != 0) {
                scheduler::resume(waiting_process);
                waiting_process = 0;
            }
        }
    }
} // namespace cosmos::devices::keyboard
