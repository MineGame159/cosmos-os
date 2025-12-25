#pragma once

#include <type_traits>

namespace stl {
    struct UseBoolFlag {};

    template <typename T, auto Sentinel = UseBoolFlag{}>
        requires std::is_trivially_copyable_v<T>
    class Optional {
        T m_value;

      public:
        /// Constructs an empty optional
        Optional() : m_value(Sentinel) {}

        /// Constructs an optional with the provided value or empty if value == Sentinel
        Optional(const T value) : m_value(value) {}

        bool has_value() const {
            return m_value != Sentinel;
        }
        bool is_empty() const {
            return m_value == Sentinel;
        }

        const T& value() const {
            return m_value;
        }
        const T& value_or(const T& fallback) const {
            return has_value() ? m_value : fallback;
        }
    };

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    class Optional<T, UseBoolFlag{}> {
        bool m_has_value;
        T m_value;

      public:
        /// Constructs an empty optional
        Optional() : m_has_value(false) {}

        /// Constructs an optional with the provided value
        Optional(const T value) : m_has_value(true), m_value(value) {}

        bool has_value() const {
            return m_has_value;
        }
        bool is_empty() const {
            return !m_has_value;
        }

        const T& value() const {
            return m_value;
        }
        const T& value_or(const T& fallback) const {
            return has_value() ? m_value : fallback;
        }
    };

    template <typename T>
        requires std::is_pointer_v<T>
    using PtrOptional = Optional<T, nullptr>;

#define OPT_VAR_CHECK(var_name, optional_expr, fallback)                                                                                   \
    const auto _opt_unique_##var_name = optional_expr;                                                                                     \
    if (_opt_unique_##var_name.is_empty()) {                                                                                               \
        return fallback;                                                                                                                   \
    }                                                                                                                                      \
    const auto var_name = _opt_unique_##var_name.value()
} // namespace stl
