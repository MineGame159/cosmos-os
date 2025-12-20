#pragma once

#include "stl/string_view.hpp"

#include <cstdint>

namespace cosmos::shell {
    using CommandFn = void (*)(stl::StringView args);

    CommandFn get_command_fn(const char* name, uint32_t name_length);
} // namespace cosmos::shell
