#include <cstdint>

static uint32_t num = 3;

uint32_t inc() {
    return ++num;
}

uint32_t main() {
    return inc();
}
