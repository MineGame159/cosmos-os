#pragma once

#include <cstdint>

namespace cosmos::shell {
    using CommandFn = void (*)(const char* args);

    CommandFn get_command_fn(const char* name, uint32_t name_length);
} // namespace cosmos::shell
