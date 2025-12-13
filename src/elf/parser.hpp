#pragma once

#include "types.hpp"
#include "vfs/types.hpp"

namespace cosmos::elf {
    Binary* parse(vfs::File* file);
} // namespace cosmos::elf
