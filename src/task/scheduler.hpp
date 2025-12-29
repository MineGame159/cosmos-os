#pragma once

#include "process.hpp"

namespace cosmos::task {
    void spawn_reaper(memory::virt::Space space);

    bool enqueue(ProcessId pid);
    bool dequeue(ProcessId pid);

    stl::Rc<Process> get_current_process();

    /// Suspends the calling process until the process passed to this function exists, returning its status code
    stl::Optional<uint64_t> join(ProcessId pid);

    void yield();
    void exit(uint64_t status);

    void suspend(UnsuspendFn unsuspend_fn, uint64_t unsuspend_data);

    void run();
} // namespace cosmos::task
