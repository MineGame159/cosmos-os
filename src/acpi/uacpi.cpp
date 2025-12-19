#include <uacpi/types.h>

#include "limine.hpp"
#include "log/log.hpp"
#include "memory/virt_range_alloc.hpp"
#include "memory/virtual.hpp"
#include "stl/utils.hpp"

extern "C" {
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
    *out_rsdp_address = cosmos::limine::get_rsdp();
    return UACPI_STATUS_OK;
}

void* uacpi_kernel_map(const uacpi_phys_addr addr, const uacpi_size len) {
    using namespace cosmos::memory::virt;

    const auto phys_start = addr / 4096ul;
    const auto phys_end = stl::ceil_div(addr + len, 4096ul);
    const auto page_count = phys_end - phys_start;

    // Allocate virtual range
    const auto virt_start = alloc_range(page_count);
    if (virt_start == 0) return nullptr;

    // Map pages
    const auto space = get_current();
    const auto status = map_pages(space, virt_start, phys_start, page_count, Flags::Write | Flags::Uncached);

    if (!status) {
        free_range(virt_start);
        return nullptr;
    }

    // Return virtual address
    return reinterpret_cast<uint8_t*>(virt_start * 4096ul) + addr % 4096ul;
}

void uacpi_kernel_unmap(void* addr, [[maybe_unused]] uacpi_size len) {
    const auto virt_start = reinterpret_cast<uint64_t>(addr) / 4096ul;
    cosmos::memory::virt::free_range(virt_start);
}

void uacpi_kernel_vlog(const uacpi_log_level level, const uacpi_char* fmt, uacpi_va_list args) {
    switch (level) {
    case UACPI_LOG_DEBUG:
    case UACPI_LOG_TRACE:
        DEBUG_ARGS(fmt, args);
        break;

    case UACPI_LOG_INFO:
        INFO_ARGS(fmt, args);
        break;

    case UACPI_LOG_WARN:
        WARN_ARGS(fmt, args);
        break;

    case UACPI_LOG_ERROR:
        ERROR_ARGS(fmt, args);
        break;
    }
}

void uacpi_kernel_log(const uacpi_log_level level, const uacpi_char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    uacpi_kernel_vlog(level, fmt, args);
    va_end(args);
}
}
