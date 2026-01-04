// result.hpp
// Minimal Result/Err for the freestanding kernel.

// Notes:
// - Provides `Result<T, E>` and `Err<E>` types for value-or-error semantics.
// - Supports manual lifetime management via union to avoid dynamic allocation.
// - Provides a minimal interface similar to `std::expected` from C++23, but suitable for freestanding/no-std environments.
// - `Result<void, E>` specialization handles operations where the success type is void.
// - Observers (`value()`, `error()`) and convenience functions (`value_or`, `error_or`, `emplace`) are provided for ergonomics.
// - All methods are `constexpr` where possible and do not rely on standard library containers.
// - Copy/move operations and assignments are carefully implemented to manage union storage safely.
// - Designed for kernel code where zero-dependency and explicit control over object lifetime are required.

#pragma once

#include <new>         // for placement new
#include <type_traits> // for type traits
#include <utility>     // for std::move, std::forward

namespace cosmos::stl {

    // ========================
    // Err wrapper
    // ========================

    /// @brief Wrapper type representing an error (used via `Err<E>`).
    /// @tparam E The error type.
    template <class E>
    struct Err {
        static_assert(!std::is_reference_v<E>, "E must not be a reference type.");
        static_assert(!std::is_void_v<E>, "E must not be void.");

        E error_; ///< Stored error.

        /// @brief Construct from lvalue error.
        /// @param e The error to store.
        constexpr explicit Err(const E& e) : error_(e) {}

        /// @brief Construct from rvalue error.
        /// @param e The error to store.
        constexpr explicit Err(E&& e) : error_(std::move(e)) {}

        /// @brief Access stored error (const lvalue).
        /// @return const reference to error.
        constexpr const E& error() const noexcept {
            return error_;
        }

        /// @brief Access stored error (lvalue).
        /// @return reference to error.
        constexpr E& error() noexcept {
            return error_;
        }
    };

    // ========================
    // Result specialization for T != void
    // ========================

    /// @brief Result type representing either a value or an error.
    /// @tparam T The success type.
    /// @tparam E The error type.
    template <class T, class E>
    class Result {
        static_assert(!std::is_reference_v<T>, "T must not be a reference type.");
        static_assert(!std::is_reference_v<E>, "E must not be a reference type.");
        static_assert(!std::is_void_v<E>, "E must not be void.");

        union Storage {
            T value; ///< Stored value
            E error; ///< Stored error

            constexpr Storage() noexcept {}
            constexpr ~Storage() noexcept {}
        } storage_;

        bool has_value_ = false; ///< true if contains value

        /// @brief Destroy currently held value or error.
        void destroy() noexcept {
            if (has_value_)
                storage_.value.~T();
            else
                storage_.error.~E();
        }

      public:
        // ========================
        // Constructors
        // ========================

        /// @brief Default constructor - constructs value via T's default ctor.
        constexpr Result() noexcept(std::is_nothrow_default_constructible_v<T>)
            requires std::is_default_constructible_v<T>
            : has_value_(true) {
            new (&storage_.value) T();
        }

        /// @brief Construct from lvalue value.
        /// @param v The value to store.
        constexpr Result(const T& v) : has_value_(true) { // NOLINT(*-explicit-constructor)
            new (&storage_.value) T(v);
        }

        /// @brief Construct from rvalue value.
        /// @param v The value to store.
        constexpr Result(T&& v) : has_value_(true) { // NOLINT(*-explicit-constructor)
            new (&storage_.value) T(std::move(v));
        }

        /// @brief Construct from const Err.
        /// @param u The Err wrapper containing the error.
        constexpr Result(const Err<E>& u) : has_value_(false) { // NOLINT(*-explicit-constructor)
            new (&storage_.error) E(u.error());
        }

        /// @brief Construct from rvalue Err.
        /// @param u The Err wrapper containing the error.
        constexpr Result(Err<E>&& u) : has_value_(false) { // NOLINT(*-explicit-constructor)
            new (&storage_.error) E(std::move(u.error()));
        }

        /// @brief Copy constructor.
        /// @param o Other Result object.
        constexpr Result(const Result& o) : has_value_(o.has_value_) {
            if (has_value_)
                new (&storage_.value) T(o.storage_.value);
            else
                new (&storage_.error) E(o.storage_.error);
        }

        /// @brief Move constructor.
        /// @param o Other Result object to move from.
        constexpr Result(Result&& o) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
            : has_value_(o.has_value_) {
            if (has_value_)
                new (&storage_.value) T(std::move(o.storage_.value));
            else
                new (&storage_.error) E(std::move(o.storage_.error));
        }

        /// @brief Destructor.
        ~Result() noexcept
            requires(!std::is_trivially_destructible_v<T> || !std::is_trivially_destructible_v<E>)
        {
            destroy();
        }

        /// @brief Destructor (trivial specialization).
        ~Result() noexcept
            requires(std::is_trivially_destructible_v<T> && std::is_trivially_destructible_v<E>)
        = default;

        // ========================
        // Assignment
        // ========================

        /// @brief Copy assignment.
        /// @param o Other Result object.
        /// @return *this
        constexpr Result& operator=(const Result& o) {
            if (this == &o) return *this;
            if (has_value_ && o.has_value_) {
                storage_.value = o.storage_.value;
            } else if (!has_value_ && !o.has_value_) {
                storage_.error = o.storage_.error;
            } else {
                destroy();
                has_value_ = o.has_value_;
                if (has_value_)
                    new (&storage_.value) T(o.storage_.value);
                else
                    new (&storage_.error) E(o.storage_.error);
            }
            return *this;
        }

        /// @brief Move assignment.
        /// @param o Other Result object.
        /// @return *this
        constexpr Result& operator=(Result&& o) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                         std::is_nothrow_move_constructible_v<E> && std::is_nothrow_move_assignable_v<T> &&
                                                         std::is_nothrow_move_assignable_v<E>) {
            if (this == &o) return *this;
            if (has_value_ && o.has_value_) {
                storage_.value = std::move(o.storage_.value);
            } else if (!has_value_ && !o.has_value_) {
                storage_.error = std::move(o.storage_.error);
            } else {
                destroy();
                has_value_ = o.has_value_;
                if (has_value_) {
                    new (&storage_.value) T(std::move(o.storage_.value));
                } else {
                    new (&storage_.error) E(std::move(o.storage_.error));
                }
            }
            return *this;
        }

        // ========================
        // Observers
        // ========================

        /// @brief Check if contains a value.
        /// @return true if value is stored
        [[nodiscard]] constexpr bool has_value() const noexcept {
            return has_value_;
        }

        /// @brief Conversion to bool.
        /// @return true if value is stored
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return has_value_;
        }

        // ========================
        // Value access
        // ========================

        /// @brief Access value (lvalue).
        /// @pre has_value() == true
        [[nodiscard]] constexpr T& value() & noexcept {
            return storage_.value;
        }

        /// @brief Access value (const lvalue).
        /// @pre has_value() == true
        [[nodiscard]] constexpr const T& value() const& noexcept {
            return storage_.value;
        }

        /// @brief Access error (lvalue).
        /// @pre has_value() == false
        [[nodiscard]] constexpr E& error() & noexcept {
            return storage_.error;
        }

        /// @brief Access error (const lvalue).
        /// @pre has_value() == false
        [[nodiscard]] constexpr const E& error() const& noexcept {
            return storage_.error;
        }

        // ========================
        // Convenience
        // ========================

        /// @brief Dereference operator (lvalue).
        [[nodiscard]] constexpr T& operator*() & noexcept {
            return storage_.value;
        }

        /// @brief Dereference operator (const lvalue).
        [[nodiscard]] constexpr const T& operator*() const& noexcept {
            return storage_.value;
        }

        /// @brief Arrow operator (non-const).
        [[nodiscard]] constexpr T* operator->() noexcept {
            return &storage_.value;
        }

        /// @brief Arrow operator (const).
        [[nodiscard]] constexpr const T* operator->() const noexcept {
            return &storage_.value;
        }

        /// @brief Return value or default if empty.
        /// @tparam U Type of default value
        /// @param default_value Fallback value
        /// @return Stored value or default
        template <class U>
        [[nodiscard]] constexpr T value_or(U&& default_value) const& noexcept(std::is_nothrow_copy_constructible_v<T> &&
                                                                              std::is_nothrow_constructible_v<T, U>) {
            return has_value_ ? storage_.value : T(std::forward<U>(default_value));
        }

        /// @brief Return value or default if empty (rvalue).
        /// @tparam U Type of default value
        /// @param default_value Fallback value
        /// @return Stored value or default
        template <class U>
        [[nodiscard]] constexpr T value_or(U&& default_value) && noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                                          std::is_nothrow_constructible_v<T, U>) {
            return has_value_ ? std::move(storage_.value) : T(std::forward<U>(default_value));
        }

        /// @brief Return error or default if value is present.
        /// @tparam U Type of default error
        /// @param default_error Fallback error
        /// @return Stored error or default
        template <class U>
        [[nodiscard]] constexpr E error_or(U&& default_error) const& noexcept(std::is_nothrow_copy_constructible_v<E> &&
                                                                              std::is_nothrow_constructible_v<E, U>) {
            return !has_value_ ? storage_.error : E(std::forward<U>(default_error));
        }

        /// @brief Return error or default if value is present (rvalue).
        /// @tparam U Type of default error
        /// @param default_error Fallback error
        /// @return Stored error or default
        template <class U>
        [[nodiscard]] constexpr E error_or(U&& default_error) && noexcept(std::is_nothrow_move_constructible_v<E> &&
                                                                          std::is_nothrow_constructible_v<E, U>) {
            return !has_value_ ? std::move(storage_.error) : E(std::forward<U>(default_error));
        }

        /// @brief Emplace a new value.
        /// @tparam Args Constructor argument types
        /// @param args Arguments to forward to T constructor
        /// @return Reference to emplaced value
        template <class... Args>
        constexpr T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
            destroy();
            has_value_ = true;
            new (&storage_.value) T(std::forward<Args>(args)...);
            return storage_.value;
        }

        /// @brief Assign a value explicitly.
        /// @tparam U Type of value to assign
        /// @param u Value to assign
        template <class U>
        constexpr void assign(U&& u) noexcept(std::is_nothrow_constructible_v<T, U> && std::is_nothrow_assignable_v<T&, U>) {
            if (has_value_) {
                storage_.value = std::forward<U>(u);
            } else {
                destroy();
                has_value_ = true;
                new (&storage_.value) T(std::forward<U>(u));
            }
        }
    };

    // ========================
    // Result specialization for void
    // ========================

    /// @brief Result specialization for void success type.
    /// @tparam E The error type.
    template <class E>
    class Result<void, E> {
        static_assert(!std::is_reference_v<E>, "E must not be a reference type.");
        static_assert(!std::is_void_v<E>, "E must not be void.");

        union Storage {
            E error; ///< Stored error

            constexpr Storage() noexcept {}
            constexpr ~Storage() noexcept {}
        } storage_;

        bool has_value_ = false; ///< true if contains success

        /// @brief Destroy currently held error.
        void destroy() noexcept {
            if (!has_value_) storage_.error.~E();
        }

      public:
        /// @brief Default success constructor.
        constexpr Result() noexcept : has_value_(true) {}

        /// @brief Construct from const Err.
        /// @param u The Err wrapper containing the error.
        constexpr Result(const Err<E>& u) : has_value_(false) { // NOLINT(*-explicit-constructor)
            new (&storage_.error) E(u.error());
        }

        /// @brief Construct from rvalue Err.
        /// @param u The Err wrapper containing the error.
        constexpr Result(Err<E>&& u) : has_value_(false) { // NOLINT(*-explicit-constructor)
            new (&storage_.error) E(std::move(u.error()));
        }

        /// @brief Copy constructor.
        /// @param o Other Result object.
        constexpr Result(const Result& o) : has_value_(o.has_value_) {
            if (!has_value_) new (&storage_.error) E(o.storage_.error);
        }

        /// @brief Move constructor.
        /// @param o Other Result object to move from.
        constexpr Result(Result&& o) noexcept(std::is_nothrow_move_constructible_v<E>) : has_value_(o.has_value_) {
            if (!has_value_) new (&storage_.error) E(std::move(o.storage_.error));
        }

        /// @brief Destructor.
        ~Result() noexcept
            requires(!std::is_trivially_destructible_v<E>)
        {
            destroy();
        }

        /// @brief Destructor (trivial specialization).
        ~Result() noexcept
            requires(std::is_trivially_destructible_v<E>)
        = default;

        /// @brief Copy assignment.
        /// @param o Other Result object.
        /// @return *this
        constexpr Result& operator=(const Result& o) {
            if (this == &o) return *this;
            if (has_value_ && !o.has_value_) {
                new (&storage_.error) E(o.storage_.error);
            } else if (!has_value_ && o.has_value_) {
                storage_.error.~E();
            } else if (!has_value_ && !o.has_value_) {
                storage_.error = o.storage_.error;
            }
            has_value_ = o.has_value_;
            return *this;
        }

        /// @brief Move assignment.
        /// @param o Other Result object.
        /// @return *this
        constexpr Result& operator=(Result&& o) noexcept(std::is_nothrow_move_constructible_v<E> && std::is_nothrow_move_assignable_v<E>) {
            if (this == &o) return *this;
            if (has_value_ && !o.has_value_) {
                new (&storage_.error) E(std::move(o.storage_.error));
            } else if (!has_value_ && o.has_value_) {
                storage_.error.~E();
            } else if (!has_value_ && !o.has_value_) {
                storage_.error = std::move(o.storage_.error);
            }
            has_value_ = o.has_value_;
            return *this;
        }

        /// @brief Check if contains a value.
        /// @return true if success is stored
        [[nodiscard]] constexpr bool has_value() const noexcept {
            return has_value_;
        }

        /// @brief Conversion to bool.
        /// @return true if success is stored
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return has_value_;
        }

        /// @brief Access error (lvalue).
        /// @pre has_value() == false
        [[nodiscard]] constexpr E& error() & noexcept {
            return storage_.error;
        }

        /// @brief Access error (const lvalue).
        /// @pre has_value() == false
        [[nodiscard]] constexpr const E& error() const& noexcept {
            return storage_.error;
        }

        /// @brief Return error or default if value is present.
        /// @tparam U Type of default error
        /// @param default_error Fallback error
        /// @return Stored error or default
        template <class U>
        [[nodiscard]] constexpr E error_or(U&& default_error) const noexcept(std::is_nothrow_constructible_v<E, U>) {
            return has_value_ ? E(std::forward<U>(default_error)) : storage_.error;
        }

        /// @brief Assign an error explicitly.
        /// @tparam U Type of error to assign
        /// @param u Error to assign
        template <class U>
        constexpr void assign(U&& u) noexcept(std::is_nothrow_constructible_v<E, U> && std::is_nothrow_assignable_v<E&, U>) {
            if (has_value_) {
                has_value_ = false;
                new (&storage_.error) E(std::forward<U>(u));
            } else {
                storage_.error = std::forward<U>(u);
            }
        }
    };

} // namespace cosmos::stl
