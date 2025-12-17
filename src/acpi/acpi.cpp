#include "acpi.hpp"

#include "utils.hpp"

#include <uacpi/tables.h>
#include <uacpi/uacpi.h>

namespace cosmos::acpi {
    constexpr uint64_t EARLY_TABLE_SIZE = 1024;
    static uint8_t early_table[EARLY_TABLE_SIZE];

    void init() {
        const auto status = uacpi_setup_early_table_access(early_table, EARLY_TABLE_SIZE);

        if (status != UACPI_STATUS_OK) {
            utils::panic(nullptr, "Failed to initialize ACPI, status: %s", uacpi_status_to_string(status));
        }
    }
} // namespace cosmos::acpi
