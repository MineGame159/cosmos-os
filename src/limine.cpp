#include "limine.hpp"
#include "memory/offsets.hpp"
#include "serial.hpp"
#include "utils.hpp"

#include <limine.h>

#include "stl/utils.hpp"

__attribute__((unused, section(".requests_start"))) //
static volatile uint64_t start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((unused, section(".requests"))) //
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(4);

__attribute__((unused, section(".requests"))) //
static volatile limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
};

__attribute__((unused, section(".requests"))) //
static volatile limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
};

__attribute__((unused, section(".requests"))) //
static volatile limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
};

__attribute__((unused, section(".requests"))) //
static volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
};

__attribute__((unused, section(".requests"))) //
static volatile limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
};

__attribute__((unused, section(".requests_end"))) //
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

namespace cosmos::limine {
    static Framebuffer fb;

    void init_framebuffer() {
        const auto limine_fb = framebuffer_request.response->framebuffers[0];

        fb = {
            .width = static_cast<uint32_t>(limine_fb->width),
            .height = static_cast<uint32_t>(limine_fb->height),
            .pitch = static_cast<uint32_t>(limine_fb->pitch) / 4,
            .pixels = limine_fb->address,
        };
    }

    void init() {
        if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
            utils::panic(nullptr, "[limine] Base revision not supported");
        }

        if (memmap_request.response == nullptr) {
            utils::panic(nullptr, "[limine] Memory ranges missing");
        }

        if (executable_address_request.response == nullptr) {
            utils::panic(nullptr, "[limine] Executable address missing");
        }

        if (hhdm_request.response == nullptr) {
            utils::panic(nullptr, "[limine] HHDM missing");
        }

        if (hhdm_request.response->offset != memory::virt::DIRECT_MAP) {
            utils::panic(nullptr, "[limine] HHDM not the same as my DIRECT_MAP");
        }

        if (framebuffer_request.response == nullptr || framebuffer_request.response->framebuffer_count < 1) {
            utils::panic(nullptr, "[limine] Framebuffer missing");
        }

        if (rsdp_request.response->address == nullptr) {
            utils::panic(nullptr, "[limine] RSDP missing");
        }

        init_framebuffer();

        serial::print("[limine] Initialized\n");
    }

    uint32_t get_memory_range_count() {
        return memmap_request.response->entry_count;
    }

    MemoryRange get_memory_range(const uint32_t index) {
        const auto entry = memmap_request.response->entries[index];

        MemoryType type;

        switch (entry->type) {
        case LIMINE_MEMMAP_USABLE:
            type = MemoryType::Usable;
            break;
        case LIMINE_MEMMAP_RESERVED:
            type = MemoryType::Reserved;
            break;
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            type = MemoryType::AcpiReclaimable;
            break;
        case LIMINE_MEMMAP_ACPI_NVS:
            type = MemoryType::AcpiNvs;
            break;
        case LIMINE_MEMMAP_BAD_MEMORY:
            type = MemoryType::BadMemory;
            break;
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            type = MemoryType::BootloaderReclaimable;
            break;
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            type = MemoryType::ExecutableAndModules;
            break;
        case LIMINE_MEMMAP_FRAMEBUFFER:
            type = MemoryType::Framebuffer;
            break;
        case LIMINE_MEMMAP_ACPI_TABLES:
            type = MemoryType::AcpiTables;
            break;
        default:
            type = MemoryType::Reserved;
            break;
        }

        auto start = entry->base;
        auto end = entry->base + entry->length;

        if (type == MemoryType::Usable) {
            start = stl::align_up(start, 4096ul);
            end = stl::align_down(end, 4096ul);
        } else {
            start = stl::align_down(start, 4096ul);
            end = stl::align_up(end, 4096ul);
        }

        return {
            .type = type,
            .first_page = start / 4096ull,
            .page_count = start >= end ? 0 : (end - start) / 4096ull,
        };
    }

    uint64_t get_memory_size() {
        const auto entry = memmap_request.response->entries[memmap_request.response->entry_count - 1];
        return entry->base + entry->length;
    }

    uint64_t get_kernel_phys() {
        return executable_address_request.response->physical_base;
    }

    uint64_t get_kernel_virt() {
        return executable_address_request.response->virtual_base;
    }

    uint64_t get_hhdm() {
        return hhdm_request.response->offset;
    }

    const Framebuffer& get_framebuffer() {
        return fb;
    }

    uint64_t get_rsdp() {
        return reinterpret_cast<uint64_t>(rsdp_request.response->address) - get_hhdm();
    }
} // namespace cosmos::limine
