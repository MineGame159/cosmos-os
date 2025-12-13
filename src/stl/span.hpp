#pragma once

#include <cstddef>

namespace cosmos::stl {
    template <typename T>
    struct Span {
        const T* data;
        size_t size;

        constexpr Span(const T* data, const size_t size) : data(data), size(size) {}

        [[nodiscard]]
        constexpr bool empty() const {
            return size == 0;
        }

        constexpr const T& operator[](const size_t i) const {
            return data[i];
        }

        [[nodiscard]]
        constexpr const T* begin() const {
            return data;
        }

        [[nodiscard]]
        constexpr const T* end() const {
            return data + size;
        }
    };
} // namespace cosmos::stl
