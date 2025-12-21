#pragma once

#include <cstdint>

enum class Sys : int64_t {
    Exit = 0,
    Yield = 1,
    Stat = 2,
    Open = 3,
    Close = 4,
    Seek = 5,
    Read = 6,
    Write = 7,
    Ioctl = 8,
    CreateDir = 9,
    Remove = 10,
    Mount = 11,
    Eventfd = 12,
    Poll = 13,
    GetCwd = 14,
    SetCwd = 15,
};

template <const Sys S>
int64_t syscall() {
    register int64_t rax asm("rax") = static_cast<int64_t>(S);

    asm volatile("syscall" : "+r"(rax) : : "rcx", "r11", "memory");
    return rax;
}

template <const Sys S>
int64_t syscall(const uint64_t arg0) {
    register int64_t rax asm("rax") = static_cast<int64_t>(S);
    register uint64_t rdi asm("rdi") = arg0;

    asm volatile("syscall" : "+r"(rax) : "r"(rdi) : "rcx", "r11", "memory");
    return rax;
}

template <const Sys S>
int64_t syscall(const uint64_t arg0, const uint64_t arg1) {
    register int64_t rax asm("rax") = static_cast<int64_t>(S);
    register uint64_t rdi asm("rdi") = arg0;
    register uint64_t rsi asm("rsi") = arg1;

    asm volatile("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi) : "rcx", "r11", "memory");
    return rax;
}

template <const Sys S>
int64_t syscall(const uint64_t arg0, const uint64_t arg1, const uint64_t arg2) {
    register int64_t rax asm("rax") = static_cast<int64_t>(S);
    register uint64_t rdi asm("rdi") = arg0;
    register uint64_t rsi asm("rsi") = arg1;
    register uint64_t rdx asm("rdx") = arg2;

    asm volatile("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx) : "rcx", "r11", "memory");
    return rax;
}

template <const Sys S>
int64_t syscall(const uint64_t arg0, const uint64_t arg1, const uint64_t arg2, const uint64_t arg3) {
    register int64_t rax asm("rax") = static_cast<int64_t>(S);
    register uint64_t rdi asm("rdi") = arg0;
    register uint64_t rsi asm("rsi") = arg1;
    register uint64_t rdx asm("rdx") = arg2;
    register uint64_t r10 asm("r10") = arg3;

    asm volatile("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10) : "rcx", "r11", "memory");
    return rax;
}

template <const Sys S>
int64_t syscall(const uint64_t arg0, const uint64_t arg1, const uint64_t arg2, const uint64_t arg3, const uint64_t arg4) {
    register int64_t rax asm("rax") = static_cast<int64_t>(S);
    register uint64_t rdi asm("rdi") = arg0;
    register uint64_t rsi asm("rsi") = arg1;
    register uint64_t rdx asm("rdx") = arg2;
    register uint64_t r10 asm("r10") = arg3;
    register uint64_t r8 asm("r8") = arg4;

    asm volatile("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
    return rax;
}

template <const Sys S>
int64_t syscall(const uint64_t arg0, const uint64_t arg1, const uint64_t arg2, const uint64_t arg3, const uint64_t arg4,
                const uint64_t arg5) {
    register int64_t rax asm("rax") = static_cast<int64_t>(S);
    register uint64_t rdi asm("rdi") = arg0;
    register uint64_t rsi asm("rsi") = arg1;
    register uint64_t rdx asm("rdx") = arg2;
    register uint64_t r10 asm("r10") = arg3;
    register uint64_t r8 asm("r8") = arg4;
    register uint64_t r9 asm("r9") = arg5;

    asm volatile("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return rax;
}

namespace sys {
    enum class FileType : uint8_t {
        Directory,
        File,
    };

    enum class Mode : uint8_t {
        Read,
        Write,
        ReadWrite,
    };

    enum class SeekType : uint8_t {
        Start,
        Current,
        End,
    };

    struct DirEntry {
        FileType type;
        char name[256];
        uint64_t name_size;
    };

    struct Stat {
        FileType type;
    };

    inline void exit(const uint64_t status) {
        syscall<Sys::Exit>(status);
    }

    inline void yield() {
        syscall<Sys::Yield>();
    }

    inline bool stat(const char* path, Stat* stat) {
        return syscall<Sys::Stat>(reinterpret_cast<uint64_t>(path), reinterpret_cast<uint64_t>(stat)) >= 0;
    }

    inline bool open(const char* path, const Mode mode, uint32_t* fd) {
        const auto result = syscall<Sys::Open>(reinterpret_cast<uint64_t>(path), static_cast<uint64_t>(mode));
        *fd = static_cast<uint32_t>(result);
        return result >= 0;
    }

    inline bool close(const uint32_t fd) {
        return syscall<Sys::Close>(fd) >= 0;
    }

    inline int64_t seek(const uint32_t fd, const SeekType type, const int64_t offset) {
        return syscall<Sys::Seek>(fd, static_cast<uint64_t>(type), offset);
    }

    inline bool read(const uint32_t fd, void* buffer, const uint64_t length, uint64_t& read) {
        const auto result = syscall<Sys::Read>(fd, reinterpret_cast<uint64_t>(buffer), length);
        read = static_cast<uint64_t>(result);
        return result >= 0;
    }

    inline bool read(const uint32_t fd, void* buffer, const uint64_t length) {
        uint64_t read_;
        return read(fd, buffer, length, read_);
    }

    inline bool write(const uint32_t fd, const void* buffer, const uint64_t length, uint64_t& written) {
        const auto result = syscall<Sys::Write>(fd, reinterpret_cast<uint64_t>(buffer), length);
        written = static_cast<uint64_t>(result);
        return result >= 0;
    }

    inline bool write(const uint32_t fd, const void* buffer, const uint64_t length) {
        uint64_t written_;
        return write(fd, buffer, length, written_);
    }

    inline uint64_t ioctl(const uint32_t fd, const uint64_t op, const uint64_t arg) {
        return syscall<Sys::Ioctl>(fd, op, arg);
    }

    inline bool create_dir(const char* path) {
        return syscall<Sys::CreateDir>(reinterpret_cast<uint64_t>(path)) >= 0;
    }

    inline bool remove(const char* path) {
        return syscall<Sys::Remove>(reinterpret_cast<uint64_t>(path)) >= 0;
    }

    inline bool mount(const char* target_path, const char* filesystem_name, const char* device_path) {
        const auto target_path_ = reinterpret_cast<uint64_t>(target_path);
        const auto filesystem_name_ = reinterpret_cast<uint64_t>(filesystem_name);
        const auto device_path_ = reinterpret_cast<uint64_t>(device_path);

        return syscall<Sys::Mount>(target_path_, filesystem_name_, device_path_) >= 0;
    }

    inline bool eventfd(uint32_t& fd) {
        const auto result = syscall<Sys::Eventfd>();
        fd = static_cast<uint32_t>(result);
        return result >= 0;
    }

    inline bool poll(const uint32_t* fds, const uint64_t count, const bool reset_signalled, uint64_t& mask) {
        return syscall<Sys::Poll>(reinterpret_cast<uint64_t>(fds), count, reset_signalled ? 1 : 0, reinterpret_cast<uint64_t>(&mask)) >= 0;
    }

    inline uint64_t get_cwd(char* buffer, const uint64_t length) {
        const auto result = syscall<Sys::GetCwd>(reinterpret_cast<uint64_t>(buffer), length);
        return result == -1 ? 0 : result;
    }

    inline bool set_cwd(const char* path) {
        return syscall<Sys::SetCwd>(reinterpret_cast<uint64_t>(path)) >= 0;
    }
} // namespace sys
