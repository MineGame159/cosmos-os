#pragma once

#include <cstddef>

extern "C" {
extern void* aligned_alloc(size_t size, size_t alignment);

extern void free(void* ptr);
}
