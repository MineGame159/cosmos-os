#pragma once

#include <type_traits>

#define ENUM_BIT_FIELD(Enum)                                                                                                               \
    constexpr Enum operator~(const Enum a) {                                                                                               \
        return static_cast<Enum>(~(static_cast<std::underlying_type_t<Enum>>(a)));                                                         \
    }                                                                                                                                      \
                                                                                                                                           \
    constexpr Enum operator|(const Enum a, const Enum b) {                                                                                 \
        return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(a) | static_cast<std::underlying_type_t<Enum>>(b));             \
    }                                                                                                                                      \
    constexpr Enum& operator|=(Enum& a, const Enum b) {                                                                                    \
        a = a | b;                                                                                                                         \
        return a;                                                                                                                          \
    }                                                                                                                                      \
                                                                                                                                           \
    constexpr Enum operator&(const Enum a, const Enum b) {                                                                                 \
        return static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(a) & static_cast<std::underlying_type_t<Enum>>(b));             \
    }                                                                                                                                      \
    constexpr Enum& operator&=(Enum& a, const Enum b) {                                                                                    \
        a = a & b;                                                                                                                         \
        return a;                                                                                                                          \
    }                                                                                                                                      \
                                                                                                                                           \
    constexpr bool operator/(const Enum a, const Enum b) {                                                                                 \
        return (a & b) == b;                                                                                                               \
    }

#define FIELD_BIT(name, field, index)                                                                                                      \
    [[nodiscard]]                                                                                                                          \
    constexpr bool name() const {                                                                                                          \
        return field & (static_cast<decltype(field)>(1u) << index);                                                                        \
    }                                                                                                                                      \
    constexpr void name(const bool value) {                                                                                                \
        if (value)                                                                                                                         \
            field |= static_cast<decltype(field)>(1u) << index;                                                                            \
        else                                                                                                                               \
            field &= ~(static_cast<decltype(field)>(1u) << index);                                                                         \
    }

#define FIELD_BITS(name, field, index, mask)                                                                                               \
    [[nodiscard]]                                                                                                                          \
    constexpr decltype(field) name() const {                                                                                               \
        return (field >> index) & mask;                                                                                                    \
    }                                                                                                                                      \
    constexpr void name(const decltype(field) value) {                                                                                     \
        field = (field & ~(mask << index)) | ((value & mask) << index);                                                                    \
    }
