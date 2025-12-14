#pragma once

#include "types.hpp"
#include "vfs/types.hpp"

namespace cosmos::elf {
    bool load(vfs::File* file, const Binary* binary);
} // namespace cosmos::elf
