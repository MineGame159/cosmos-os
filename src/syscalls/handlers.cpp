#include "log/log.hpp"
#include "memory/heap.hpp"
#include "memory/offsets.hpp"
#include "task/event.hpp"
#include "task/pipe.hpp"
#include "task/scheduler.hpp"
#include "utils.hpp"
#include "vfs/path.hpp"
#include "vfs/vfs.hpp"

#include <cstdint>

namespace cosmos::syscalls {
    // Helpers

    static stl::StringView get_string_view(const uint64_t arg) {
        const auto ptr = reinterpret_cast<const char*>(arg);
        size_t length = 0;

        for (;;) {
            if (memory::virt::is_invalid_user(arg + length)) return stl::StringView("", 0);
            if (ptr[length] == '\0') break;
            length++;
        }

        return stl::StringView(ptr, length);
    }

    static void free_string(const stl::StringView str) {
        memory::heap::free(const_cast<char*>(str.data()));
    }

    // Syscall handlers

    int64_t exit(const uint64_t status) {
        task::exit(status);
        return 0;
    }

    int64_t yield() {
        task::yield();
        return 0;
    }

    int64_t stat(const uint64_t path_, const uint64_t stat_) {
        if (memory::virt::is_invalid_user(stat_)) return -1;
        if (memory::virt::is_invalid_user(stat_ + sizeof(vfs::Stat))) return -1;

        const auto path = get_string_view(path_);
        const auto stat = reinterpret_cast<vfs::Stat*>(stat_);
        const auto process = task::get_current_process();

        const auto abs_path = vfs::resolve(process->cwd, path);
        const auto result = vfs::stat(abs_path, *stat);

        free_string(abs_path);
        return result ? 0 : -1;
    }

    int64_t open(const uint64_t path_, const uint64_t mode_) {
        const auto path = get_string_view(path_);
        const auto mode = static_cast<vfs::Mode>(mode_);
        const auto process = task::get_current_process();

        const auto abs_path = vfs::resolve(process->cwd, path);

        const auto file = vfs::open(abs_path, mode);
        if (!file.valid()) {
            free_string(abs_path);
            return -1;
        }

        const auto fd = process->add_fd(file);
        if (fd.is_empty()) {
            free_string(abs_path);
            return -1;
        }

        free_string(abs_path);
        return fd.value();
    }

    int64_t close(const uint64_t fd) {
        const auto process = task::get_current_process();
        const auto file = process->remove_fd(fd);

        return file.valid() ? 0 : -1;
    }

    int64_t seek(const uint64_t fd, const uint64_t type_, const uint64_t offset_) {
        const auto type = static_cast<vfs::SeekType>(type_);
        const auto offset = static_cast<int64_t>(offset_);

        const auto file = task::get_current_process()->get_file(fd);
        if (!file.valid()) return -1;

        return file->ops->seek(file, type, offset);
    }

    int64_t read(const uint64_t fd, const uint64_t buffer_, const uint64_t length) {
        if (memory::virt::is_invalid_user(buffer_)) return -1;
        if (memory::virt::is_invalid_user(buffer_ + length - 1)) return -1;

        const auto buffer = reinterpret_cast<void*>(buffer_);

        const auto file = task::get_current_process()->get_file(fd);
        if (!file.valid()) return -1;

        return file->ops->read(file, buffer, length);
    }

    int64_t write(const uint64_t fd, const uint64_t buffer_, const uint64_t length) {
        if (memory::virt::is_invalid_user(buffer_)) return -1;
        if (memory::virt::is_invalid_user(buffer_ + length - 1)) return -1;

        const auto buffer = reinterpret_cast<const void*>(buffer_);

        const auto file = task::get_current_process()->get_file(fd);
        if (!file.valid()) return -1;

        return file->ops->write(file, buffer, length);
    }

    int64_t ioctl(const uint64_t fd, const uint64_t op, const uint64_t arg) {
        const auto file = task::get_current_process()->get_file(fd);
        if (!file.valid()) return -1;

        return file->ops->ioctl(file, op, arg);
    }

    int64_t create_dir(const uint64_t path_) {
        const auto path = get_string_view(path_);
        const auto process = task::get_current_process();

        const auto abs_path = vfs::resolve(process->cwd, path);

        const auto result = vfs::create_dir(abs_path);

        free_string(abs_path);
        return result ? 0 : -1;
    }

    int64_t remove(const uint64_t path_) {
        const auto path = get_string_view(path_);
        const auto process = task::get_current_process();

        const auto abs_path = vfs::resolve(process->cwd, path);

        const auto result = vfs::remove(abs_path);

        free_string(abs_path);
        return result ? 0 : -1;
    }

    int64_t mount(const uint64_t target_path_, const uint64_t filesystem_name_, const uint64_t device_path_) {
        const auto target_path = get_string_view(target_path_);
        const auto filesystem_name = get_string_view(filesystem_name_);
        const auto device_path = get_string_view(device_path_);

        const auto process = task::get_current_process();
        const auto cwd = process->cwd;

        const auto abs_target_path = vfs::resolve(cwd, target_path);
        const auto abs_device_path = vfs::resolve(cwd, device_path);

        return vfs::mount(abs_target_path, filesystem_name, abs_device_path) != nullptr ? 0 : -1;
    }

    int64_t eventfd() {
        uint32_t fd;

        if (!task::create_event(nullptr, 0, fd).valid()) {
            return -1;
        }

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
        const auto process = task::get_current_process();

        stl::Rc<vfs::File> event_files[64];

        for (auto i = 0u; i < count; i++) {
            event_files[i] = process->get_file(fds[i]);
        }

        *mask = task::wait_on_events(event_files, count, reset_signalled);
        return 0;
    }

    int64_t pipe(const uint64_t fds_) {
        if (memory::virt::is_invalid_user(fds_)) return -1;
        if (memory::virt::is_invalid_user(fds_ + 2 * sizeof(uint32_t))) return -1;

        const auto fds = reinterpret_cast<uint32_t*>(fds_);

        // Create pipe files
        stl::Rc<vfs::File> read_file;
        stl::Rc<vfs::File> write_file;

        if (!task::create_pipe(read_file, write_file)) {
            return -1;
        }

        // Allocate FDs
        const auto process = task::get_current_process();

        const auto read_fd = process->add_fd(read_file);
        const auto write_fd = process->add_fd(write_file);

        if (read_fd.is_empty() || write_fd.is_empty()) {
            return -1;
        }

        // Return
        fds[0] = read_fd.value();
        fds[1] = write_fd.value();

        return 0;
    }

    int64_t fork(const task::StackFrame& frame) {
        // Setup child stack frame
        auto child_frame = frame;
        child_frame.rax = 0;

        // Fork process
        const auto process = task::get_current_process();
        OPT_VAR_CHECK(child_pid, process->fork(child_frame), -1);

        task::enqueue(child_pid);
        return child_pid;
    }

    int64_t get_cwd(const uint64_t buffer_, const uint64_t length) {
        if (memory::virt::is_invalid_user(buffer_)) return -1;
        if (memory::virt::is_invalid_user(buffer_ + length - 1)) return -1;

        const auto buffer = reinterpret_cast<char*>(buffer_);
        const auto process = task::get_current_process();

        const auto cwd = process->cwd;
        if (length < cwd.size() + 1) return -1;

        utils::memcpy(buffer, cwd.data(), cwd.size());
        buffer[cwd.size()] = '\0';

        return cwd.size();
    }

    int64_t set_cwd(const uint64_t path_) {
        const auto path = get_string_view(path_);
        const auto process = task::get_current_process();

        const auto abs_path = vfs::resolve(process->cwd, path);

        vfs::Stat stat;
        if (!vfs::stat(abs_path, stat) || stat.type != vfs::NodeType::Directory) return -1;

        return process->set_cwd(abs_path) ? 0 : -1;
    }

    uint64_t join(const uint64_t pid_) {
        if (pid_ > 0xFFFFFFFF) return 0xFFFFFFFFFFFFFFFF;
        const auto pid = static_cast<task::ProcessId>(pid_);

        return task::join(pid).value_or(0xFFFFFFFFFFFFFFFF);
    }

    // Handler

    extern "C" void syscall_handler(const uint64_t number, task::StackFrame* frame) {
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
            CASE_1(9, create_dir)
            CASE_1(10, remove)
            CASE_3(11, mount)
            CASE_0(12, eventfd)
            CASE_4(13, poll)
            CASE_1(14, pipe)

        case 15:
            frame->rax = fork(*frame);
            break;

            CASE_2(16, get_cwd)
            CASE_1(17, set_cwd)
            CASE_1(18, join)

        default:
            ERROR("Invalid syscalls %llu from process %lu", number, task::get_current_process()->id);
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
