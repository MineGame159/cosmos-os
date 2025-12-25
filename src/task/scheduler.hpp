#pragma once

#include "process.hpp"

namespace cosmos::task {
    bool enqueue(ProcessId pid);
    bool dequeue(ProcessId pid);

    Process* get_current_process();

    /// Suspends the calling process until the process passed to this function exists, returning its status code
    stl::Optional<uint64_t> join(ProcessId pid);

    void yield();
    void exit(uint64_t status);

    void suspend();
    void resume(ProcessId pid);

    void run();
} // namespace cosmos::task
