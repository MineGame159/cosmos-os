#pragma once
#include <compare>
#include <cstddef>

namespace cosmos::stl {
    struct string_view {
      private:
        const char* data_ = nullptr;
        size_t size_ = 0;

      public:
        constexpr string_view() noexcept = default;
        constexpr string_view(const char* s, const size_t n) noexcept : data_(s), size_(n) {}
        explicit constexpr string_view(const char* s) noexcept : data_(s), size_(s ? calculate_len(s) : 0) {}

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

        [[nodiscard]] constexpr string_view remove_prefix(const size_t n) const noexcept {
            if (n >= size_) return {};
            return { data_ + n, size_ - n };
        }

        [[nodiscard]] constexpr string_view remove_suffix(const size_t n) const noexcept {
            if (n >= size_) return {};
            return { data_, size_ - n };
        }

        // Operations

        [[nodiscard]] constexpr bool starts_with(const string_view other) const noexcept {
            const size_t n = other.size();
            if (n > size_) return false;
            for (size_t i = 0; i < n; ++i) {
                if (data_[i] != other[i]) return false;
            }
            return true;
        }

        [[nodiscard]] constexpr bool starts_with(const char* s) const noexcept {
            return starts_with(string_view(s));
        }

        [[nodiscard]] constexpr bool ends_with(const string_view other) const noexcept {
            const size_t n = other.size();
            if (n > size_) return false;
            const size_t off = size_ - n;
            for (size_t i = 0; i < n; ++i) {
                if (data_[off + i] != other[i]) return false;
            }
            return true;
        }

        [[nodiscard]] constexpr bool ends_with(const char* s) const noexcept {
            return ends_with(string_view(s));
        }

        [[nodiscard]] constexpr bool contains(const string_view other) const noexcept {
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
            return contains(string_view(s));
        }

        // Comparison

        [[nodiscard]] constexpr std::strong_ordering operator<=>(const string_view other) const noexcept {
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

    constexpr std::strong_ordering operator<=>(const string_view lhs, const char* rhs) noexcept {
        return lhs <=> string_view(rhs);
    }

    constexpr std::strong_ordering operator<=>(const char* lhs, const string_view rhs) noexcept {
        return string_view(lhs) <=> rhs;
    }

} // namespace cosmos::stl
