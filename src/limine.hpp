#pragma once

#include <cstdint>

namespace cosmos::limine {
    enum class MemoryType {
        Usable,
        Reserved,
        AcpiReclaimable,
        AcpiNvs,
        BadMemory,
        BootloaderReclaimable,
        ExecutableAndModules,
        Framebuffer,
        AcpiTables,
    };

    inline bool memory_type_ram(const MemoryType type) {
        switch (type) {
        case MemoryType::Reserved:
        case MemoryType::Framebuffer:
            return false;
        default:
            return true;
        }
    }

    struct MemoryRange {
        MemoryType type;
        uint64_t first_page;
        uint64_t page_count;
    };

    struct Framebuffer {
        uint32_t width;
        uint32_t height;
        uint32_t pitch;
        void* pixels;
    };

    void init();

    uint32_t get_memory_range_count();
    MemoryRange get_memory_range(uint32_t index);

    uint64_t get_memory_size();

    uint64_t get_kernel_phys();
    uint64_t get_kernel_virt();

    uint64_t get_hhdm();

    const Framebuffer& get_framebuffer();

    uint64_t get_rsdp();
} // namespace cosmos::limine
