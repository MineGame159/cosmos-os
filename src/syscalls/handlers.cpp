#include "log/log.hpp"
#include "memory/offsets.hpp"
#include "scheduler/event.hpp"
#include "scheduler/scheduler.hpp"
#include "vfs/vfs.hpp"

#include <cstdint>

struct SyscallFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, rflags, rsp;
};

namespace cosmos::syscalls {
    // Helpers

    stl::StringView get_string_view(const uint64_t arg) {
        const auto ptr = reinterpret_cast<const char*>(arg);
        size_t length = 0;

        for (;;) {
            if (memory::virt::is_invalid_user(arg + length)) return stl::StringView("", 0);
            if (ptr[length] == '\0') break;
            length++;
        }

        return stl::StringView(ptr, length);
    }

    // Syscall handlers

    int64_t exit(const uint64_t status) {
        scheduler::exit(status);
        return 0;
    }

    int64_t yield() {
        scheduler::yield();
        return 0;
    }

    int64_t stat(const uint64_t path_, const uint64_t stat_) {
        if (memory::virt::is_invalid_user(stat_)) return -1;
        if (memory::virt::is_invalid_user(stat_ + sizeof(vfs::Stat))) return -1;

        const auto path = get_string_view(path_);
        const auto stat = reinterpret_cast<vfs::Stat*>(stat_);

        const auto result = vfs::stat(path, *stat);
        return result ? 0 : -1;
    }

    int64_t open(const uint64_t path_, const uint64_t mode_) {
        const auto path = get_string_view(path_);
        const auto mode = static_cast<vfs::Mode>(mode_);
        const auto pid = scheduler::get_current_process();

        const auto file = vfs::open(path, mode);
        if (file == nullptr) return -1;

        const auto fd = scheduler::add_fd(pid, file);
        if (fd == 0xFFFFFFFF) {
            vfs::close(file);
            return -1;
        }

        return fd;
    }

    int64_t close(const uint64_t fd) {
        const auto pid = scheduler::get_current_process();

        const auto file = scheduler::remove_fd(pid, fd);
        if (file == nullptr) return -1;

        vfs::close(file);
        return 0;
    }

    int64_t seek(const uint64_t fd, const uint64_t type_, const uint64_t offset_) {
        const auto type = static_cast<vfs::SeekType>(type_);
        const auto offset = static_cast<int64_t>(offset_);
        const auto pid = scheduler::get_current_process();

        const auto file = scheduler::get_file(pid, fd);
        if (file == nullptr) return -1;

        return file->ops->seek(file, type, offset);
    }

    int64_t read(const uint64_t fd, const uint64_t buffer_, const uint64_t length) {
        if (memory::virt::is_invalid_user(buffer_)) return -1;
        if (memory::virt::is_invalid_user(buffer_ + length - 1)) return -1;

        const auto buffer = reinterpret_cast<void*>(buffer_);
        const auto pid = scheduler::get_current_process();

        const auto file = scheduler::get_file(pid, fd);
        if (file == nullptr) return -1;

        return file->ops->read(file, buffer, length);
    }

    int64_t write(const uint64_t fd, const uint64_t buffer_, const uint64_t length) {
        if (memory::virt::is_invalid_user(buffer_)) return -1;
        if (memory::virt::is_invalid_user(buffer_ + length - 1)) return -1;

        const auto buffer = reinterpret_cast<const void*>(buffer_);
        const auto pid = scheduler::get_current_process();

        const auto file = scheduler::get_file(pid, fd);
        if (file == nullptr) return -1;

        return file->ops->write(file, buffer, length);
    }

    int64_t ioctl(const uint64_t fd, const uint64_t op, const uint64_t arg) {
        const auto pid = scheduler::get_current_process();

        const auto file = scheduler::get_file(pid, fd);
        if (file == nullptr) return -1;

        return file->ops->ioctl(file, op, arg);
    }

    int64_t eventfd() {
        uint32_t fd;

        const auto event_file = scheduler::create_event(nullptr, 0, fd);
        if (event_file == nullptr) return -1;

        return fd;
    }

    int64_t poll(const uint64_t fds_, const uint64_t count, const uint64_t reset_signalled_, const uint64_t mask_) {
        if (memory::virt::is_invalid_user(fds_)) return -1;
        if (memory::virt::is_invalid_user(fds_ + count * sizeof(uint32_t))) return -1;
        if (memory::virt::is_invalid_user(mask_)) return -1;
        if (memory::virt::is_invalid_user(mask_ + sizeof(uint64_t))) return -1;

        if (count > 64) return -1;

        const auto fds = reinterpret_cast<uint32_t*>(fds_);
        const auto reset_signalled = reset_signalled_ != 0;
        const auto mask = reinterpret_cast<uint64_t*>(mask_);
        const auto pid = scheduler::get_current_process();

        vfs::File* event_files[64];

        for (auto i = 0u; i < count; i++) {
            event_files[i] = scheduler::get_file(pid, fds[i]);
        }

        *mask = scheduler::wait_on_events(event_files, count, reset_signalled);
        return 0;
    }

    // Handler

    extern "C" void syscall_handler(const uint64_t number, SyscallFrame* frame) {
#define CASE_0(number, handler)                                                                                                            \
    case number:                                                                                                                           \
        frame->rax = handler();                                                                                                            \
        break;
#define CASE_1(number, handler)                                                                                                            \
    case number:                                                                                                                           \
        frame->rax = handler(frame->rdi);                                                                                                  \
        break;
#define CASE_2(number, handler)                                                                                                            \
    case number:                                                                                                                           \
        frame->rax = handler(frame->rdi, frame->rsi);                                                                                      \
        break;
#define CASE_3(number, handler)                                                                                                            \
    case number:                                                                                                                           \
        frame->rax = handler(frame->rdi, frame->rsi, frame->rdx);                                                                          \
        break;
#define CASE_4(number, handler)                                                                                                            \
    case number:                                                                                                                           \
        frame->rax = handler(frame->rdi, frame->rsi, frame->rdx, frame->r10);                                                              \
        break;
#define CASE_5(number, handler)                                                                                                            \
    case number:                                                                                                                           \
        frame->rax = handler(frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8);                                                   \
        break;
#define CASE_6(number, handler)                                                                                                            \
    case number:                                                                                                                           \
        frame->rax = handler(frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);                                        \
        break;

        switch (number) {
            CASE_1(0, exit)
            CASE_0(1, yield)
            CASE_2(2, stat)
            CASE_2(3, open)
            CASE_1(4, close)
            CASE_3(5, seek)
            CASE_3(6, read)
            CASE_3(7, write)
            CASE_3(8, ioctl)
            CASE_0(9, eventfd)
            CASE_4(10, poll)

        default:
            ERROR("Invalid syscalls %llu from process %llu", number, scheduler::get_current_process());
            frame->rax = -1;
            break;
        }

#undef CASE_6
#undef CASE_5
#undef CASE_4
#undef CASE_3
#undef CASE_2
#undef CASE_1
#undef CASE_0
    }
} // namespace cosmos::syscalls
