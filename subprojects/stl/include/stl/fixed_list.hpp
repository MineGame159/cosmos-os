#pragma once

#include "utils.hpp"

#include <cstddef>
#include <cstdint>

namespace stl {
    template <Equatable T, const size_t N, const T EMPTY>
    struct FixedList {
        struct Iterator {
            const T* items;
            size_t max_count;
            size_t index;

            bool operator==(const Iterator& other) const {
                return index == other.index;
            }

            const T& operator*() const {
                return items[index];
            }

            const T* operator->() const {
                return &items[index];
            }

            Iterator& operator++() {
                do {
                    index++;
                } while (index < max_count && items[index] == EMPTY);

                return *this;
            }

            const T* operator++(int) {
                const auto item = &items[index];
                ++*this;
                return item;
            }
        };

      private:
        static constexpr T EMPTY_FIELD = EMPTY;

      public:
        T items[N];
        size_t max_count;

        FixedList() : max_count(0) {
            for (size_t i = 0; i < N; i++) {
                items[i] = EMPTY;
            }
        }

        Iterator begin() const {
            size_t i = 0;

            while (i < max_count && items[i] == EMPTY) {
                i++;
            }

            return { items, max_count, i };
        }

        Iterator end() const {
            return { items, max_count, max_count };
        }

        const T& get(const size_t index) const {
            if (index >= max_count) return EMPTY_FIELD;
            return items[index];
        }

        T set(const size_t index, const T item) {
            const auto prev = items[index];

            items[index] = item;

            if (index >= max_count) {
                max_count = index + 1;
            }

            return prev;
        }

        intptr_t add(T item) {
            for (size_t i = 0; i < N; i++) {
                if (items[i] == EMPTY) {
                    items[i] = item;

                    if (i >= max_count) {
                        max_count = i + 1;
                    }

                    return i;
                }
            }

            return -1;
        }

        bool try_add(T*& item, size_t& index) {
            for (size_t i = 0; i < N; i++) {
                if (items[i] == EMPTY) {
                    item = &items[i];
                    index = i;

                    if (i >= max_count) {
                        max_count = i + 1;
                    }

                    return true;
                }
            }

            item = nullptr;
            index = -1;

            return false;
        }

        intptr_t index_of(T item) const {
            for (size_t i = 0; i < N; i++) {
                if (items[i] == item) {
                    return i;
                }
            }

            return -1;
        }

        T remove_at(size_t index) {
            if (index >= max_count) return EMPTY;

            const auto item = items[index];
            items[index] = EMPTY;

            if (index == max_count - 1) {
                for (size_t i = 0; i < max_count; i++) {
                    if (items[max_count - 1 - i] != EMPTY) break;
                    max_count--;
                }
            }

            return item;
        }

        T remove(T item) {
            const auto index = index_of(item);
            if (index == -1) return EMPTY;

            return remove_at(index);
        }
    };
} // namespace stl
