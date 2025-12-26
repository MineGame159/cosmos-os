#pragma once

#include "types.hpp"
#include "vfs/types.hpp"

namespace cosmos::elf {
    Binary* parse(const stl::Rc<vfs::File>& file);
} // namespace cosmos::elf
