#pragma once

#include "memory/virtual.hpp"
#include "types.hpp"
#include "vfs/types.hpp"

namespace cosmos::elf {
    bool load(memory::virt::Space space, vfs::File* file, const Binary* binary);
} // namespace cosmos::elf
