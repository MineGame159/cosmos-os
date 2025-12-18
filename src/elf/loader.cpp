#include "loader.hpp"

#include "log/log.hpp"
#include "memory/offsets.hpp"
#include "memory/physical.hpp"
#include "memory/virtual.hpp"
#include "utils.hpp"

namespace cosmos::elf {
    bool load_header_load(const memory::virt::Space space, vfs::File* file, const ProgramHeader& header) {
        // Validate header
        if (header.file_size > header.virt_size) {
            ERROR("Corrupted header, file_size > virt_size");
            return false;
        }

        // Calculate addresses
        const auto start_addr = header.virt_offset;
        const auto end_addr = start_addr + header.virt_size;

        const auto virt_start = start_addr / 4096ul;
        const auto virt_end = utils::ceil_div(end_addr, 4096ul);
        const auto count = virt_end - virt_start;

        if (count == 0) return true;

        if (memory::virt::is_kernel(virt_start * 4096ul) || memory::virt::is_kernel(virt_end * 4096ul)) {
            ERROR("Invalid virtual address in header, tried to write to kernel");
            return false;
        }

        // Allocate physical pages
        const auto phys = memory::phys::alloc_pages(count) / 4096ul;

        if (phys == 0) {
            ERROR("Failed to allocate %d physical pages", count);
            return false;
        }

        // Map virtual address space
        auto flags = memory::virt::Flags::Write | memory::virt::Flags::User;
        if (header.flags / ProgramHeaderFlags::Execute) flags |= memory::virt::Flags::Execute;

        if (!memory::virt::map_pages(space, virt_start, phys, count, flags)) {
            ERROR("Failed to map virtual address space");
            memory::phys::free_pages(phys, count);
            return false;
        }

        // Read header data
        file->ops->seek(file, vfs::SeekType::Start, static_cast<int64_t>(header.file_offset));

        const auto ptr = reinterpret_cast<uint8_t*>(memory::virt::DIRECT_MAP + phys * 4096 + start_addr % 4096);

        if (file->ops->read(file, ptr, header.file_size) != header.file_size) {
            ERROR("Failed to ready program header data");
            memory::phys::free_pages(phys, count);
            return false;
        }

        // Zero remaining memory
        const auto remaining = virt_end * 4096ul - (start_addr + header.file_size);
        utils::memset(ptr + header.file_size, 0, remaining);

        return true;
    }

    bool load(const memory::virt::Space space, vfs::File* file, const Binary* binary) {
        for (const auto& header : binary->program_headers) {
            if (header.type == ProgramHeaderType::Load) {
                if (!load_header_load(space, file, header)) return false;
            }
        }

        return true;
    }
} // namespace cosmos::elf
