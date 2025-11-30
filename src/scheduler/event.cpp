#include "event.hpp"

#include "memory/heap.hpp"
#include "private.hpp"

namespace cosmos::scheduler {
    EventHandle create_event(void (*destroy_fn)(uint64_t data), const uint64_t destroy_data) {
        const auto event = memory::heap::alloc<Event>();

        event->destroy_fn = destroy_fn;
        event->destroy_data = destroy_data;

        event->signalled = false;
        event->waiting_process = nullptr;

        return reinterpret_cast<uint64_t>(event);
    }

    bool destroy_event(const EventHandle handle) {
        const auto event = reinterpret_cast<Event*>(handle);
        if (event->waiting_process != nullptr) return false;

        if (event->destroy_fn != nullptr) event->destroy_fn(event->destroy_data);
        memory::heap::free(event);

        return true;
    }

    void signal_event(const EventHandle handle) {
        const auto event = reinterpret_cast<Event*>(handle);

        event->signalled = true;
        if (event->waiting_process != nullptr) event->waiting_process->event_signalled = true;
    }

    bool check_event(const EventHandle handle) {
        const auto event = reinterpret_cast<Event*>(handle);
        return event->signalled;
    }

    bool reset_event(const EventHandle handle) {
        const auto event = reinterpret_cast<Event*>(handle);
        if (event->waiting_process != nullptr) return false;

        event->signalled = false;
        return true;
    }

    uint64_t get_signalled_mask(Event** events, const uint32_t count, const bool reset_signalled) {
        uint64_t mask = 0;

        for (auto i = 0u; i < count; i++) {
            const auto event = events[i];

            if (event->signalled) {
                mask |= 1ull << i;
                if (reset_signalled) event->signalled = false;
            }

            event->waiting_process = nullptr;
        }

        return mask;
    }

    uint64_t wait_on_events(EventHandle* handles, const uint32_t count, const bool reset_signalled) {
        if (count > 64) return 0;
        asm volatile("cli" ::: "memory");

        const auto process = reinterpret_cast<Process*>(get_current_process());
        const auto events = reinterpret_cast<Event**>(handles);

        for (auto i = 0u; i < count; i++) {
            if (events[i]->signalled) {
                const auto mask = get_signalled_mask(events, count, reset_signalled);

                asm volatile("sti" ::: "memory");
                return mask;
            }
        }

        for (auto i = 0u; i < count; i++) {
            events[i]->waiting_process = process;
        }

        process->events = events;
        process->event_count = count;
        process->event_signalled = false;

        process->state = State::SuspendedEvents;
        yield();
        asm volatile("cli" ::: "memory");

        const auto mask = get_signalled_mask(events, count, reset_signalled);

        asm volatile("sti" ::: "memory");
        return mask;
    }
} // namespace cosmos::scheduler
