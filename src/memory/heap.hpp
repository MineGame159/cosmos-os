#pragma once

#include <cstdint>

namespace cosmos::memory::heap {
    void init();

    void* alloc(uint64_t size, uint64_t alignment);
    void free(void* ptr);

    inline void* alloc(const uint64_t size) {
        return alloc(size, 1);
    }

    template <typename T>
    T* alloc(const uint64_t additional_size = 0) {
        return static_cast<T*>(alloc(sizeof(T) + additional_size, alignof(T)));
    }

    template <typename T>
    T* alloc_array(const uint32_t count) {
        return static_cast<T*>(alloc(sizeof(T) * count, alignof(T)));
    }
} // namespace cosmos::memory::heap
