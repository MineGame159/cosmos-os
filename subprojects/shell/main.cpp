#include "syscalls.hpp"

static uint32_t num = 3;

uint32_t inc() {
    return ++num;
}

extern "C" void _start() {
    const auto number = inc();
    syscall<Sys::Exit>(number);
}
