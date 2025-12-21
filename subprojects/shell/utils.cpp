#include <cstdint>

extern "C" void memcpy(void* dst, const void* src, const uint64_t count) {
    const auto dst_ = static_cast<uint8_t*>(dst);
    const auto src_ = static_cast<const uint8_t*>(src);

    for (auto i = 0u; i < count; i++) {
        dst_[i] = src_[i];
    }
}
