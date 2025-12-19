#pragma once

#include <concepts>

namespace stl {
    // Concepts

    template <typename T>
    concept Comparable = requires(const T t) {
        { t < t } -> std::convertible_to<bool>;
        { t <= t } -> std::convertible_to<bool>;
        { t > t } -> std::convertible_to<bool>;
        { t >= t } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept Equatable = requires(const T t) {
        { t == t } -> std::convertible_to<bool>;
        { t != t } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept Number = requires(const T t) {
        { t + t } -> std::convertible_to<T>;
        { t - t } -> std::convertible_to<T>;
        { t * t } -> std::convertible_to<T>;
        { t / t } -> std::convertible_to<T>;
        { t % t } -> std::convertible_to<T>;
    };

    template <typename T>
    concept BinaryNumber = Number<T> && requires(const T t) {
        { t & t } -> std::convertible_to<T>;
        { t | t } -> std::convertible_to<T>;
        { t ^ t } -> std::convertible_to<T>;
        { ~t } -> std::convertible_to<T>;
    };

    // Functions

    template <Comparable T>
    T min(T a, T b) {
        return a < b ? a : b;
    }

    template <Comparable T>
    T max(T a, T b) {
        return a > b ? a : b;
    }

    template <Number T>
    T ceil_div(T a, T b) {
        return (a + b - 1) / b;
    }

    template <BinaryNumber T>
    T align_up(T value, T alignment) {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }

    template <BinaryNumber T>
    T align_down(T value, T alignment) {
        return value & ~(alignment - 1);
    }
} // namespace stl
