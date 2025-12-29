#include "event.hpp"

#include "memory/heap.hpp"
#include "scheduler.hpp"

namespace cosmos::task {
    static uint64_t event_seek([[maybe_unused]] const stl::Rc<vfs::File>& file, [[maybe_unused]] vfs::SeekType type,
                               [[maybe_unused]] int64_t offset) {
        return 0;
    }

    static uint64_t event_read(const stl::Rc<vfs::File>& file, void* buffer, const uint64_t length) {
        if (length != sizeof(uint64_t)) return 0;
        const auto event = reinterpret_cast<Event*>(*file + 1);

        if (event->number == 0) {
            wait_on_events(&file, 1, false);
        }

        *static_cast<uint64_t*>(buffer) = event->number;
        event->number = 0;

        return sizeof(uint64_t);
    }

    static uint64_t event_write(const stl::Rc<vfs::File>& file, const void* buffer, const uint64_t length) {
        if (length != sizeof(uint64_t)) return 0;

        asm volatile("cli" ::: "memory");
        const auto event = reinterpret_cast<Event*>(*file + 1);

        event->number += *static_cast<const uint64_t*>(buffer);
        if (event->waiting_process != nullptr) event->waiting_process->unsuspend_data = 1;

        asm volatile("sti" ::: "memory");
        return sizeof(uint64_t);
    }

    static uint64_t event_ioctl([[maybe_unused]] const stl::Rc<vfs::File>& file, [[maybe_unused]] uint64_t op,
                                [[maybe_unused]] uint64_t arg) {
        return vfs::IOCTL_UNKNOWN;
    }

    static constexpr vfs::FileOps event_ops = {
        .seek = event_seek,
        .read = event_read,
        .write = event_write,
        .ioctl = event_ioctl,
    };

    static void event_close(vfs::File* file) {
        const auto event = reinterpret_cast<Event*>(file + 1);

        if (event->close_fn != nullptr) {
            event->close_fn(event->close_data);
        }
    }

    // Header

    stl::Rc<vfs::File> create_event(void (*close_fn)(uint64_t data), const uint64_t close_data, const vfs::FileFlags flags, uint32_t& fd) {
        const auto file = stl::Rc<vfs::File>::alloc(sizeof(Event));

        if (!file.valid()) {
            fd = 0xFFFFFFFF;
            return {};
        }

        file->ops = &event_ops;
        file->on_close = event_close;
        file->node = nullptr;
        file->mode = vfs::Mode::ReadWrite;
        file->flags = flags;
        file->cursor = 0;

        fd = get_current_process()->add_fd(file).value_or(0xFFFFFFFF);

        if (fd == 0xFFFFFFFF) {
            return {};
        }

        const auto event = reinterpret_cast<Event*>(*file + 1);

        event->close_fn = close_fn;
        event->close_data = close_data;
        event->number = 0;
        event->waiting_process = nullptr;

        return file;
    }

    static uint64_t get_signalled_mask(const stl::Rc<vfs::File>* event_files, const uint32_t count, const bool reset_signalled) {
        uint64_t mask = 0;

        for (auto i = 0u; i < count; i++) {
            if (event_files[i] == nullptr) continue;
            const auto event = reinterpret_cast<Event*>(*event_files[i] + 1);

            if (event->number > 0) {
                mask |= 1ull << i;
                if (reset_signalled) event->number = 0;
            }

            event->waiting_process = nullptr;
        }

        return mask;
    }

    static bool wait_on_events_unsuspend(const uint64_t signalled) {
        return signalled != 0;
    }

    uint64_t wait_on_events(const stl::Rc<vfs::File>* event_files, uint32_t count, bool reset_signalled) {
        if (count > 64) return 0;
        asm volatile("cli" ::: "memory");

        const auto process = get_current_process();

        for (auto i = 0u; i < count; i++) {
            if (event_files[i] == nullptr) continue;
            const auto event = reinterpret_cast<Event*>(*event_files[i] + 1);

            if (event->number > 0) {
                const auto mask = get_signalled_mask(event_files, count, reset_signalled);

                asm volatile("sti" ::: "memory");
                return mask;
            }
        }

        for (auto i = 0u; i < count; i++) {
            if (event_files[i] == nullptr) continue;
            const auto event = reinterpret_cast<Event*>(*event_files[i] + 1);

            event->waiting_process = *process;
        }

        process->event_files = event_files;
        process->event_count = count;

        suspend(wait_on_events_unsuspend, 0);
        asm volatile("cli" ::: "memory");

        const auto mask = get_signalled_mask(event_files, count, reset_signalled);

        asm volatile("sti" ::: "memory");
        return mask;
    }
} // namespace cosmos::task
