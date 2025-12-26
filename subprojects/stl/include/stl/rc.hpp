#pragma once

#include "mem.hpp"


#include <algorithm>
#include <concepts>

namespace stl {
    template <typename T>
    concept RefCountable = requires(T t) {
        { t.ref_count } -> std::same_as<size_t&>;
        t.destroy();
    };

    template <RefCountable T>
    class Rc {
        T* ptr;

      public:
        Rc() noexcept : ptr(nullptr) {};

        Rc(T* ptr) noexcept : ptr(ptr) {
            // ReSharper disable once CppExpressionWithoutSideEffects
            ref();
        }

        Rc(const Rc& other) : ptr(other.ptr) {
            // ReSharper disable once CppExpressionWithoutSideEffects
            ref();
        }

        Rc(Rc&& other) noexcept : ptr(other.ptr) {
            other.ptr = nullptr;
        }

        ~Rc() {
            deref();
        }

        static Rc alloc(const size_t additional_size = 0) {
            const auto ptr = static_cast<T*>(aligned_alloc(sizeof(T) + additional_size, alignof(T)));
            ptr->ref_count = 0;

            return ptr;
        }

        Rc& operator=(Rc other) noexcept {
            swap(other);
            return *this;
        }

        T* operator*() const noexcept {
            return ptr;
        }

        T* operator->() const noexcept {
            return ptr;
        }

        bool operator==(const Rc& other) const noexcept {
            return other.ptr == ptr;
        }

        bool valid() const noexcept {
            return ptr != nullptr;
        }

        T* ref() const noexcept {
            if (ptr != nullptr) {
                __atomic_add_fetch(&ptr->ref_count, 1, __ATOMIC_RELAXED);
            }

            return ptr;
        }

        void deref() const noexcept {
            if (ptr != nullptr && __atomic_sub_fetch(&ptr->ref_count, 1, __ATOMIC_ACQ_REL) == 0) {
                ptr->destroy();
            }
        }

      private:
        void swap(Rc& other) noexcept {
            std::swap(ptr, other.ptr);
        }
    };
} // namespace stl
