// ctype.hpp
// ASCII-only character classification helpers for the freestanding kernel.
//
// Notes:
// - These functions assume ASCII semantics (0x00..0x7F).
//   They are not Unicode-aware and do not attempt to classify multi-byte UTF-8 code points.
// - Inputs are cast to `unsigned char` internally to avoid surprises on platforms where `char` is signed;
//   this matches the conventional contract of the C `is*` family (which accepts `unsigned char` values or EOF).
// - Intended for kernel/no-std environments where the C locale and <ctype.h> are unavailable.

#pragma once

namespace cosmos::stl {
    [[nodiscard]] constexpr bool is_alnum(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return (uc >= '0' && uc <= '9') || (uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z');
    }

    [[nodiscard]] constexpr bool is_alpha(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return (uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z');
    }

    [[nodiscard]] constexpr bool is_lower(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return uc >= 'a' && uc <= 'z';
    }

    [[nodiscard]] constexpr bool is_upper(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return uc >= 'A' && uc <= 'Z';
    }

    [[nodiscard]] constexpr bool is_digit(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return uc >= '0' && uc <= '9';
    }

    [[nodiscard]] constexpr bool is_xdigit(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return (uc >= '0' && uc <= '9') || (uc >= 'A' && uc <= 'F') || (uc >= 'a' && uc <= 'f');
    }

    [[nodiscard]] constexpr bool is_cntrl(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return uc <= 0x1F || uc == 0x7F;
    }

    [[nodiscard]] constexpr bool is_graph(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);

        if (uc >= '0' && uc <= '9') return true;
        if (uc >= 'A' && uc <= 'Z') return true;
        if (uc >= 'a' && uc <= 'z') return true;
        if ((uc >= '!' && uc <= '/') || (uc >= ':' && uc <= '@') || (uc >= '[' && uc <= '`') || (uc >= '{' && uc <= '~')) return true;
        return false;
    }

    [[nodiscard]] constexpr bool is_space(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return uc == ' ' || uc == '\t' || uc == '\n' || uc == '\r' || uc == '\f' || uc == '\v';
    }

    [[nodiscard]] constexpr bool is_blank(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return uc == ' ' || uc == '\t';
    }

    [[nodiscard]] constexpr bool is_print(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return uc >= 0x20 && uc <= 0x7E;
    }

    [[nodiscard]] constexpr bool is_punct(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        return (uc >= '!' && uc <= '/') || (uc >= ':' && uc <= '@') || (uc >= '[' && uc <= '`') || (uc >= '{' && uc <= '~');
    }

    [[nodiscard]] constexpr char to_upper(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        if (uc >= 'a' && uc <= 'z') {
            return static_cast<char>(uc - ('a' - 'A'));
        }
        return c;
    }

    [[nodiscard]] constexpr char to_lower(const char c) noexcept {
        const auto uc = static_cast<unsigned char>(c);
        if (uc >= 'A' && uc <= 'Z') {
            return static_cast<char>(uc + ('a' - 'A'));
        }
        return c;
    }
} // namespace cosmos::stl
