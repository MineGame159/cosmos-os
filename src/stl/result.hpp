// expected.hpp
// Minimal Expected/Unexpected for the freestanding kernel.

// Notes:
// - Provides `Expected<T, E>` and `Unexpected<E>` types for value-or-error semantics.
// - Supports manual lifetime management via union to avoid dynamic allocation.
// - Provides a minimal interface similar to `std::expected` from C++23, but suitable for freestanding/no-std environments.
// - `Expected<void, E>` specialization handles operations where the success type is void.
// - Observers (`value()`, `error()`) and convenience functions (`value_or`, `error_or`, `emplace`) are provided for ergonomics.
// - All methods are `constexpr` where possible and do not rely on standard library containers.
// - Copy/move operations and assignments are carefully implemented to manage union storage safely.
// - Designed for kernel code where zero-dependency and explicit control over object lifetime are required.

#pragma once


#include <new>         // for placement new
#include <type_traits> // for type traits

namespace cosmos::stl {

    // ========================
    // Unexpected wrapper
    // ========================

    /// @brief Wrapper type representing an unexpected error.
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
        constexpr explicit Err(E&& e) : error_(static_cast<E&&>(e)) {}

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
    // Expected specialization for T != void
    // ========================

    /// @brief Expected type representing either a value or an error.
    /// @tparam T The success type.
    /// @tparam E The error type.
    template <class T, class E>
    class Result {
        static_assert(!std::is_reference_v<T>, "T must not be a reference type.");
        static_assert(!std::is_reference_v<E>, "E must not be a reference type.");

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

        /// @brief Construct from lvalue value.
        /// @param v The value to store.
        constexpr Result(const T& v) : has_value_(true) { // NOLINT(*-explicit-constructor)
            new (&storage_.value) T(v);
        }

        /// @brief Construct from rvalue value.
        /// @param v The value to store.
        constexpr Result(T&& v) : has_value_(true) { // NOLINT(*-explicit-constructor)
            new (&storage_.value) T(static_cast<T&&>(v));
        }

        /// @brief Construct from const Unexpected.
        /// @param u The unexpected error.
        constexpr Result(const Err<E>& u) : has_value_(false) { // NOLINT(*-explicit-constructor)
            new (&storage_.error) E(u.error());
        }

        /// @brief Construct from rvalue Unexpected.
        /// @param u The unexpected error.
        constexpr Result(Err<E>&& u) : has_value_(false) { // NOLINT(*-explicit-constructor)
            new (&storage_.error) E(static_cast<E&&>(u.error()));
        }

        /// @brief Copy constructor.
        /// @param o Other Expected object.
        constexpr Result(const Result& o) : has_value_(o.has_value_) {
            if (has_value_)
                new (&storage_.value) T(o.storage_.value);
            else
                new (&storage_.error) E(o.storage_.error);
        }

        /// @brief Move constructor.
        /// @param o Other Expected object to move from.
        constexpr Result(Result&& o) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
            : has_value_(o.has_value_) {
            if (has_value_)
                new (&storage_.value) T(static_cast<T&&>(o.storage_.value));
            else
                new (&storage_.error) E(static_cast<E&&>(o.storage_.error));
        }

        /// @brief Destructor.
        ~Result() noexcept {
            destroy();
        }

        // ========================
        // Assignment
        // ========================

        /// @brief Copy assignment.
        /// @param o Other Expected object.
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
        /// @param o Other Expected object.
        /// @return *this
        constexpr Result& operator=(Result&& o) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                         std::is_nothrow_move_constructible_v<E> && std::is_nothrow_move_assignable_v<T> &&
                                                         std::is_nothrow_move_assignable_v<E>) {
            if (this == &o) return *this;
            if (has_value_ && o.has_value_) {
                storage_.value = static_cast<T&&>(o.storage_.value);
            } else if (!has_value_ && !o.has_value_) {
                storage_.error = static_cast<E&&>(o.storage_.error);
            } else {
                destroy();
                has_value_ = o.has_value_;
                if (has_value_)
                    new (&storage_.value) T(static_cast<T&&>(o.storage_.value));
                else
                    new (&storage_.error) E(static_cast<E&&>(o.storage_.error));
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

        [[nodiscard]] constexpr T& operator*() & noexcept {
            return storage_.value;
        }
        [[nodiscard]] constexpr const T& operator*() const& noexcept {
            return storage_.value;
        }
        [[nodiscard]] constexpr T* operator->() noexcept {
            return &storage_.value;
        }
        [[nodiscard]] constexpr const T* operator->() const noexcept {
            return &storage_.value;
        }

        /// @brief Return value or default if empty.
        template <class U>
        [[nodiscard]] constexpr T value_or(U&& default_value) const& noexcept {
            return has_value_ ? storage_.value : T(static_cast<U&&>(default_value));
        }

        /// @brief Return value or default if empty (rvalue).
        template <class U>
        [[nodiscard]] constexpr T value_or(U&& default_value) && noexcept {
            return has_value_ ? static_cast<T&&>(storage_.value) : T(static_cast<U&&>(default_value));
        }

        /// @brief Return error or default if value is present.
        template <class U>
        [[nodiscard]] constexpr E error_or(U&& default_error) const& noexcept {
            return !has_value_ ? storage_.error : E(static_cast<U&&>(default_error));
        }

        /// @brief Return error or default if value is present (rvalue).
        template <class U>
        [[nodiscard]] constexpr E error_or(U&& default_error) && noexcept {
            return !has_value_ ? static_cast<E&&>(storage_.error) : E(static_cast<U&&>(default_error));
        }

        /// @brief Emplace a new value.
        template <class... Args>
        [[nodiscard]] constexpr T& emplace(Args&&... args) noexcept {
            destroy();
            has_value_ = true;
            new (&storage_.value) T(static_cast<Args&&>(args)...);
            return storage_.value;
        }
    };

    // ========================
    // Expected specialization for void
    // ========================

    /// @brief Expected specialization for void success type.
    /// @tparam E The error type.
    template <class E>
    class Result<void, E> {
        static_assert(!std::is_reference_v<E>, "E must not be a reference type.");

        union Storage {
            E error;

            constexpr Storage() noexcept {}
            constexpr ~Storage() noexcept {}
        } storage_;

        bool has_value_ = false;

        void destroy() noexcept {
            if (!has_value_) storage_.error.~E();
        }

      public:
        /// @brief Default success constructor.
        constexpr Result() noexcept : has_value_(true) {}

        /// @brief Construct from const Unexpected.
        constexpr Result(const Err<E>& u) : has_value_(false) { // NOLINT(*-explicit-constructor)
            new (&storage_.error) E(u.error());
        }

        /// @brief Construct from rvalue Unexpected.
        constexpr Result(Err<E>&& u) : has_value_(false) { // NOLINT(*-explicit-constructor)
            new (&storage_.error) E(static_cast<E&&>(u.error()));
        }

        /// @brief Copy constructor.
        constexpr Result(const Result& o) : has_value_(o.has_value_) {
            if (!has_value_) new (&storage_.error) E(o.storage_.error);
        }

        /// @brief Move constructor.
        constexpr Result(Result&& o) noexcept(std::is_nothrow_move_constructible_v<E>) : has_value_(o.has_value_) {
            if (!has_value_) new (&storage_.error) E(static_cast<E&&>(o.storage_.error));
        }

        ~Result() noexcept {
            destroy();
        }

        /// @brief Copy assignment.
        constexpr Result& operator=(const Result& o) {
            if (this == &o) return *this;
            if (has_value_ && !o.has_value_) {
                new (&storage_.error) E(o.storage_.error);
            } else if (!has_value_ && !o.has_value_) {
                storage_.error = o.storage_.error;
            }
            has_value_ = o.has_value_;
            return *this;
        }

        /// @brief Move assignment.
        constexpr Result& operator=(Result&& o) noexcept(std::is_nothrow_move_constructible_v<E> && std::is_nothrow_move_assignable_v<E>) {
            if (this == &o) return *this;
            if (has_value_ && !o.has_value_) {
                new (&storage_.error) E(static_cast<E&&>(o.storage_.error));
            } else if (!has_value_ && !o.has_value_) {
                storage_.error = static_cast<E&&>(o.storage_.error);
            }
            has_value_ = o.has_value_;
            return *this;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return has_value_;
        }
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return has_value_;
        }

        [[nodiscard]] constexpr E& error() & noexcept {
            return storage_.error;
        }
        [[nodiscard]] constexpr const E& error() const& noexcept {
            return storage_.error;
        }

        [[nodiscard]] constexpr E error_or(E&& default_error) const noexcept {
            return has_value_ ? static_cast<E&&>(default_error) : storage_.error;
        }
    };

} // namespace cosmos::stl
