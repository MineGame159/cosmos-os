#include "process.hpp"

#include "elf/loader.hpp"
#include "elf/parser.hpp"
#include "log/log.hpp"
#include "memory/heap.hpp"
#include "memory/offsets.hpp"
#include "memory/physical.hpp"
#include "utils.hpp"
#include "vfs/vfs.hpp"

namespace cosmos::task {
    static stl::FixedList<Process*, 256, nullptr> processes = {};

    __attribute__((naked)) void user_entry_stub() {
        asm volatile("swapgs; iretq");
    }

    void setup_dummy_frame(StackFrame& frame, const ProcessFn fn) {
        for (auto i = 0ul; i < 15; i++) {
            frame[i] = i;
        }

        frame.rip = reinterpret_cast<uint64_t>(fn);
        frame.rflags = 0x202;
        frame.user_rsp = memory::virt::LOWER_HALF_END;
    }

    stl::Optional<ProcessId> create_process(const memory::virt::Space space, const Land land, const bool alloc_user_stack,
                                            const StackFrame& frame, const stl::StringView cwd) {
        // Allocate id
        Process** process_ptr;
        size_t index;

        if (!processes.try_add(process_ptr, index)) {
            ERROR("Failed to create process, too many processes");
            return {};
        }

        // Allocate process
        const auto process = memory::heap::alloc<Process>();
        *process_ptr = process;

        process->id = index;

        // Set basic fields
        process->ref_count = 1;

        process->land = land;

        process->state = State::Waiting;
        process->status = 0xFFFFFFFF;

        process->space = space;

        process->joining_with = nullptr;

        process->event_files = nullptr;
        process->event_count = 0;
        process->event_signalled = false;

        process->fd_table = {};

        // Allocate kernel stack
        process->kernel_stack = memory::heap::alloc(KERNEL_STACK_SIZE, 16);

        if (process->kernel_stack == nullptr) {
            ERROR("Failed to allocate memory for kernel stack");

            processes.remove(process);
            memory::heap::free(process);

            return {};
        }

        process->kernel_stack_rsp = 0;

        // Allocate user stack
        if (land == Land::User) {
            constexpr auto virt = (memory::virt::LOWER_HALF_END - USER_STACK_SIZE) / 4096ul;

            if (alloc_user_stack) {
                process->user_stack_phys = memory::phys::alloc_pages(USER_STACK_SIZE / 4096ul);

                if (process->user_stack_phys == 0) {
                    ERROR("Failed to allocate memory for user stack");

                    memory::heap::free(process->kernel_stack);
                    processes.remove(process);
                    memory::heap::free(process);

                    return {};
                }

                const auto phys = process->user_stack_phys / 4096ul;
                constexpr auto flags = memory::virt::Flags::Write | memory::virt::Flags::User;
                const auto status = memory::virt::map_pages(space, virt, phys, USER_STACK_SIZE / 4096ul, flags);

                if (!status) {
                    memory::phys::free_pages(phys, USER_STACK_SIZE / 4096ul);
                    memory::heap::free(process->kernel_stack);
                    processes.remove(process);
                    memory::heap::free(process);

                    return {};
                }
            } else {
                process->user_stack_phys = memory::virt::get_phys(virt);
            }
        } else {
            process->user_stack_phys = 0;
        }

        // Setup kernel stack
        auto stack = reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(process->kernel_stack) + KERNEL_STACK_SIZE);

        if (land == Land::Kernel) {
            *--stack = frame.rip;    // Entry point
            *--stack = frame.rflags; // Flags
        } else {
            *--stack = 24 | 3;         // GDT - User data
            *--stack = frame.user_rsp; // User RSP
            *--stack = frame.rflags;   // Flags
            *--stack = 32 | 3;         // GDT - User code
            *--stack = frame.rip;      // Entry point

            *--stack = reinterpret_cast<uint64_t>(user_entry_stub); // Entry point stub
            *--stack = 0x2;                                         // Flags
        }

        for (auto i = 0ul; i < 15; i++) {
            *--stack = frame[i];
        }

        process->kernel_stack_rsp = reinterpret_cast<uint64_t>(stack);

        // Set cwd
        process->cwd = stl::StringView("", 0);
        process->set_cwd(cwd);

        return process->id;
    }

    stl::Optional<ProcessId> create_process(const ProcessFn fn, const Land land, const stl::StringView cwd) {
        // Create address space
        const auto space = memory::virt::create();
        if (space == 0) return {};

        // Setup stack frame
        StackFrame frame;
        setup_dummy_frame(frame, fn);

        // Create process
        const auto pid = create_process(space, land, true, frame, cwd);
        if (pid.is_empty()) memory::virt::destroy(space);

        return pid;
    }

    stl::Optional<ProcessId> create_process(const stl::StringView path, const stl::StringView cwd) {
        // Open file
        const auto file = vfs::open(path, vfs::Mode::Read);

        if (file == nullptr) {
            ERROR("Failed to open file");
            return {};
        }

        // Parse ELF binary
        const auto binary = elf::parse(file);

        if (binary == nullptr) {
            vfs::close(file);
            return {};
        }

        // Create process
        const auto pid = create_process(reinterpret_cast<ProcessFn>(binary->virt_entry), Land::User, cwd);

        if (pid.is_empty()) {
            memory::heap::free(binary);
            vfs::close(file);
            return {};
        }

        INFO("Creating process %llu for file %s", pid.value(), path.data());
        const auto process = get_process(pid.value()).value();

        // Load ELF binary
        if (!elf::load(process->space, file, binary)) {
            memory::heap::free(binary);
            vfs::close(file);
            return {};
        }

        // Cleanup
        memory::heap::free(binary);
        vfs::close(file);

        return pid;
    }

    stl::PtrOptional<Process*> get_process(const ProcessId id) {
        return processes.get(id);
    }

    ProcessId Process::ref() {
        ref_count++;
        return id;
    }

    void Process::unref() {
        ref_count--;

        if (ref_count == 0) {
            for (const auto file : fd_table) {
                vfs::close(file);
            }

            memory::heap::free(const_cast<char*>(cwd.data()));
            memory::virt::destroy(space);
            memory::heap::free(kernel_stack);

            processes.remove_at(id);
            memory::heap::free(this);
        }
    }

    bool Process::set_cwd(const stl::StringView path) {
        if (path.empty()) return false;

        const auto old_cwd = cwd;

        const auto cwd_ptr = memory::heap::alloc_array<char>(path.size() + 1);
        cwd = stl::StringView(cwd_ptr, path.size());

        utils::memcpy(cwd_ptr, path.data(), path.size());
        cwd_ptr[path.size()] = '\0';

        if (!old_cwd.empty()) {
            memory::heap::free(const_cast<char*>(old_cwd.data()));
        }

        return true;
    }

    stl::Optional<uint32_t> Process::add_fd(vfs::File* file) {
        const auto index = fd_table.add(file);
        return index != -1 ? stl::Optional<uint32_t>(index) : stl::Optional<uint32_t>();
    }

    stl::PtrOptional<vfs::File*> Process::get_file(const uint32_t fd) const {
        return fd_table.get(fd);
    }

    stl::PtrOptional<vfs::File*> Process::remove_fd(const uint32_t fd) {
        return fd_table.remove_at(fd);
    }

    stl::Optional<ProcessId> Process::fork(const StackFrame& frame) const {
        if (land != Land::User) {
            ERROR("Can only fork user-land processes");
            return {};
        }

        // Fork address space
        const auto new_space = memory::virt::fork(space);
        if (new_space == 0) return {};

        // Create process
        const auto pid = create_process(new_space, land, false, frame, cwd);

        if (pid.is_empty()) {
            memory::virt::destroy(new_space);
            return {};
        }

        const auto process = get_process(pid.value()).value();

        // Duplicate file descriptors
        for (auto it = fd_table.begin(); it != fd_table.end(); ++it) {
            const auto file = vfs::duplicate(*it);
            process->fd_table.set(it.index, file);
        }

        return process->id;
    }
} // namespace cosmos::task
