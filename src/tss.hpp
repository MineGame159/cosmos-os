#pragma once

#include <cstdint>

namespace cosmos::tss {
    void init();

    void set_rsp(uint8_t level, uint64_t rsp);

    uint64_t get_address();
    uint64_t get_size();
} // namespace cosmos::tss
