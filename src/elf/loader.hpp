#pragma once

#include "memory/virtual.hpp"
#include "types.hpp"
#include "vfs/types.hpp"

namespace cosmos::elf {
    bool load(memory::virt::Space space, const stl::Rc<vfs::File>& file, const Binary* binary);
} // namespace cosmos::elf
