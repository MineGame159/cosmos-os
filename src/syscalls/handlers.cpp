#include "log/log.hpp"
#include "scheduler/scheduler.hpp"

#include <cstdint>

struct SyscallFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, rflags, rsp;
};

namespace cosmos::syscalls {
    int64_t exit(const uint64_t status) {
        scheduler::exit(status);
        return 0;
    }

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
            CASE_1(60, exit)

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
