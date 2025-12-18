#include "log/log.hpp"
#include "memory/offsets.hpp"
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

    int64_t open_file(const uint64_t path_, const uint64_t mode_) {
        const auto path = get_string_view(path_);
        const auto mode = static_cast<vfs::Mode>(mode_);
        const auto pid = scheduler::get_current_process();

        const auto file = vfs::open_file(path, mode);
        if (file == nullptr) return -1;

        const auto fd = scheduler::add_fd(pid, file);
        if (fd == 0xFFFFFFFF) {
            vfs::close_file(file);
            return -1;
        }

        return fd;
    }

    int64_t close_file(const uint64_t fd) {
        const auto pid = scheduler::get_current_process();

        const auto file = scheduler::remove_fd(pid, fd);
        if (file == nullptr) return -1;

        vfs::close_file(file);
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
            CASE_2(2, open_file)
            CASE_1(3, close_file)
            CASE_3(4, seek)
            CASE_3(5, read)
            CASE_3(6, write)
            CASE_3(7, ioctl)

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
