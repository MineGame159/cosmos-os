#include "utils.hpp"

namespace cosmos::utils {
    void halt() {
        for (;;) {
            asm volatile("hlt");
        }
    }
} // namespace cosmos::utils
