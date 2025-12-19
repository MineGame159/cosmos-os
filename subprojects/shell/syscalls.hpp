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
    Eventfd = 9,
    Poll = 10,
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
