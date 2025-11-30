#pragma once

#include <cstdint>

namespace cosmos::scheduler {
    using EventHandle = uint64_t;

    EventHandle create_event(void (*destroy_fn)(uint64_t data), uint64_t destroy_data);
    bool destroy_event(EventHandle handle);

    void signal_event(EventHandle handle);
    bool check_event(EventHandle handle);
    bool reset_event(EventHandle handle);

    uint64_t wait_on_events(EventHandle* handles, uint32_t count, bool reset_signalled);
} // namespace cosmos::scheduler
