#pragma once

#include "stl/string_view.hpp"

using CommandFn = void (*)(stl::StringView args);

CommandFn get_command_fn(stl::StringView name);
