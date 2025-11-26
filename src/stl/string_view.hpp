// string_view.hpp
// Byte-oriented string_view for the freestanding kernel.
//
// Notes:
// - Minimal replacement for `std::string_view`: a non-owning, byte-oriented view into character data.
// - Methods on this type return new views (immutable) rather than mutating the existing object.
//   This differs from `std::string_view::remove_prefix`/`remove_suffix` (which mutate in-place).
// - This type treats the underlying bytes as a sequence of ASCII/UTF-8 bytes;
//   it does not decode multi-byte Unicode code points.
//   Character-classification helpers come from `ctype.hpp` and assume ASCII semantics where appropriate.
// - An empty view may have a nullptr `data()` pointer;
//   callers should respect `size()` rather than relying on a null terminator.
// - Intended for kernel/no-std environments where <string_view> is not available.

#pragma once

#include "ctype.hpp"
#include <compare>
#include <cstddef>
#include <limits>

namespace cosmos::stl {
    struct StringView {
      private:
        const char* data_ = nullptr;
        size_t size_ = 0;

      public:
        constexpr StringView() noexcept = default;
        constexpr StringView(const char* s, const size_t n) noexcept : data_(s), size_(n) {}
        explicit constexpr StringView(const char* s) noexcept : data_(s), size_(s ? calculate_len(s) : 0) {}

        // Accessors

        [[nodiscard]] constexpr const char* data() const noexcept {
            return data_;
        }
        [[nodiscard]] constexpr size_t size() const noexcept {
            return size_;
        }
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }
        constexpr const char& operator[](const size_t i) const noexcept {
            return data_[i];
        }

        // Element access

        [[nodiscard]] constexpr const char& front() const noexcept {
            return data_[0];
        }
        [[nodiscard]] constexpr const char& back() const noexcept {
            return data_[size_ - 1];
        }

        // Iterator access

        [[nodiscard]] constexpr const char* begin() const noexcept {
            return data_;
        }
        [[nodiscard]] constexpr const char* end() const noexcept {
            return data_ + size_;
        }

        // Modifiers (immutable)

        [[nodiscard]] constexpr StringView remove_prefix(const size_t n) const noexcept {
            if (n >= size_) return {};
            return { data_ + n, size_ - n };
        }

        [[nodiscard]] constexpr StringView remove_suffix(const size_t n) const noexcept {
            if (n >= size_) return {};
            return { data_, size_ - n };
        }

        // `substr` returns a substring starting at `pos` with length `n`.
        // If `pos` is out of range, an empty string_view is returned.
        // If `n` exceeds the available length, it is clamped to the maximum possible.
        [[nodiscard]] constexpr StringView substr(const size_t pos, const size_t n = SIZE_MAX) const noexcept {
            if (pos >= size_) return {};
            const size_t max_len = size_ - pos;
            const size_t len = (n < max_len) ? n : max_len;
            return { data_ + pos, len };
        }

        // `slice` returns a substring following Python semantics:
        // - `start` and `stop` are signed indices; negative values count from the end.
        // - `stop` is exclusive.
        // - If `stop` is not specified (default sentinel), it is treated as the end of the string.
        // - Indices are clamped to the valid range [0, size()] after normalization.
        // - If start >= stop after normalization and clamping, the result is empty.
        [[nodiscard]] constexpr StringView slice(ptrdiff_t start = 0,
                                                  ptrdiff_t stop = std::numeric_limits<ptrdiff_t>::max()) const noexcept {
            const auto sz = static_cast<ptrdiff_t>(size_);

            // Normalize negative indices (count from end)
            if (start < 0) start += sz;
            if (stop < 0) stop += sz;

            // If stop was omitted, treat as end
            if (stop == std::numeric_limits<ptrdiff_t>::max()) stop = sz;

            // Clamp to [0, sz]
            if (start < 0) start = 0;
            if (stop < 0) stop = 0;
            if (start > sz) start = sz;
            if (stop > sz) stop = sz;

            // If empty or reversed range, return empty
            if (start >= stop) return {};

            const auto s = static_cast<size_t>(start);
            const auto len = static_cast<size_t>(stop - start);
            return { data_ + s, len };
        }

        [[nodiscard]] constexpr StringView trim() const noexcept {
            size_t start = 0;
            size_t end = size_;

            while (start < end && is_space(data_[start])) {
                ++start;
            }
            while (end > start && is_space(data_[end - 1])) {
                --end;
            }
            return { data_ + start, end - start };
        }

        [[nodiscard]] constexpr StringView ltrim() const noexcept {
            size_t start = 0;
            const size_t end = size_;

            while (start < end && is_space(data_[start])) {
                ++start;
            }
            return { data_ + start, end - start };
        }

        [[nodiscard]] constexpr StringView rtrim() const noexcept {
            const size_t start = 0;
            size_t end = size_;

            while (end > start && is_space(data_[end - 1])) {
                --end;
            }
            return { data_ + start, end - start };
        }

        // Operations

        [[nodiscard]] constexpr bool starts_with(const StringView other) const noexcept {
            const size_t n = other.size();
            if (n > size_) return false;
            for (size_t i = 0; i < n; ++i) {
                if (data_[i] != other[i]) return false;
            }
            return true;
        }

        [[nodiscard]] constexpr bool starts_with(const char* s) const noexcept {
            return starts_with(StringView(s));
        }

        [[nodiscard]] constexpr bool ends_with(const StringView other) const noexcept {
            const size_t n = other.size();
            if (n > size_) return false;
            const size_t off = size_ - n;
            for (size_t i = 0; i < n; ++i) {
                if (data_[off + i] != other[i]) return false;
            }
            return true;
        }

        [[nodiscard]] constexpr bool ends_with(const char* s) const noexcept {
            return ends_with(StringView(s));
        }

        [[nodiscard]] constexpr bool contains(const StringView other) const noexcept {
            const size_t n = other.size();
            if (n == 0) return true;
            if (n > size_) return false;

            for (size_t i = 0; i <= size_ - n; ++i) {
                bool match = true;
                for (size_t j = 0; j < n; ++j) {
                    if (data_[i + j] != other[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return true;
            }
            return false;
        }

        [[nodiscard]] constexpr bool contains(const char* s) const noexcept {
            return contains(StringView(s));
        }

        // Comparison

        [[nodiscard]] constexpr std::strong_ordering operator<=>(const StringView other) const noexcept {
            const size_t n = (size_ < other.size_) ? size_ : other.size_;
            for (size_t i = 0; i < n; ++i) {
                if (data_[i] < other.data_[i]) return std::strong_ordering::less;
                if (data_[i] > other.data_[i]) return std::strong_ordering::greater;
            }
            if (size_ < other.size_) return std::strong_ordering::less;
            if (size_ > other.size_) return std::strong_ordering::greater;
            return std::strong_ordering::equal;
        }

      private:
        static constexpr size_t calculate_len(const char* s) noexcept {
            size_t len = 0;
            while (s[len] != '\0') {
                ++len;
            }
            return len;
        }
    };

    constexpr std::strong_ordering operator<=>(const StringView lhs, const char* rhs) noexcept {
        return lhs <=> StringView(rhs);
    }

    constexpr std::strong_ordering operator<=>(const char* lhs, const StringView rhs) noexcept {
        return StringView(lhs) <=> rhs;
    }

} // namespace cosmos::stl
