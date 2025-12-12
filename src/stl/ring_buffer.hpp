#pragma once

#include "utils.hpp"

#include <type_traits>

namespace cosmos::stl {
    template <typename T, std::size_t N>
    struct RingBuffer {
        static_assert(std::is_trivially_copyable_v<T>, "RingBuffer requires T to be trivially copyable");

        T data[N];

        volatile std::size_t write_index = 0;
        volatile std::size_t read_index = 0;

        [[nodiscard]]
        std::size_t size() const {
            return (write_index + N - read_index) % N;
        }

        [[nodiscard]]
        std::size_t remaining() const {
            return (N - 1) - ((write_index + N - read_index) % N);
        }

        bool add(const T item) {
            const auto next_write_index = (write_index + 1) % N;

            if (next_write_index != read_index) {
                data[write_index] = item;
                write_index = next_write_index;

                return true;
            }

            return false;
        }

        bool add(const T* items, const std::size_t count) {
            if (count > remaining()) return false;

            // Calculate how much we can copy before hitting the end of the physical array
            const auto first_chunk = utils::min(count, N - write_index);
            const auto second_chunk = count - first_chunk;

            // Copy first chunk (Write -> End of Array)
            utils::memcpy(&data[write_index], items, first_chunk * sizeof(T));

            // Copy second chunk (Start of Array -> Remainder), if needed
            if (second_chunk > 0) {
                utils::memcpy(&data[0], items + first_chunk, second_chunk * sizeof(T));
            }

            write_index = (write_index + count) % N;
            return true;
        }

        bool try_get(T& item) {
            if (read_index != write_index) {
                item = data[read_index];
                read_index = (read_index + 1) % N;

                return true;
            }

            return false;
        }

        std::size_t try_get(T* dst, const std::size_t capacity) {
            // Determine how many items we can actually read
            const auto count = utils::min(capacity, size());
            if (count == 0) return 0;

            // Calculate how much is contiguous in memory before wrapping
            const auto first_chunk = utils::min(count, N - read_index);
            const auto second_chunk = count - first_chunk;

            // Copy first chunk (Read -> End of Array)
            utils::memcpy(dst, &data[read_index], first_chunk * sizeof(T));

            // Copy second chunk (Start of Array -> Remainder), if needed
            if (second_chunk > 0) {
                utils::memcpy(dst + first_chunk, &data[0], second_chunk * sizeof(T));
            }

            read_index = (read_index + count) % N;
            return count;
        }

        void reset() {
            write_index = 0;
            read_index = 0;
        }
    };
} // namespace cosmos::stl
