#include "virt_range_alloc.hpp"

#include "log/log.hpp"
#include "offsets.hpp"
#include "stl/linked_list.hpp"

namespace cosmos::memory::virt {
    struct Region {
        bool used : 1;
        uint64_t size : 63;
    };

    static stl::LinkedList<Region> regions = {};

    void init_range_alloc() {
        *regions.push_back_alloc() = {
            .used = false,
            .size = GB / 4096ul,
        };
    }

    void alloc_from_region(stl::LinkedList<Region>::Iterator& it, const uint64_t page_count) {
        if (it->size == page_count) {
            it->used = true;
            return;
        }

        *regions.insert_after_alloc(it) = {
            .used = false,
            .size = it->size - page_count,
        };

        it->used = true;
        it->size = page_count;
    }

    void merge(const stl::LinkedList<Region>::Iterator& it) {
        const auto prev = it.prev;
        const auto current = it.node;
        const auto next = current->next;

        if (next != nullptr && !next->item.used) {
            current->item.size += next->item.size;
            regions.remove_free(current, next);
        }

        if (prev != nullptr && !prev->item.used) {
            prev->item.size += current->item.size;
            regions.remove_free(prev, current);
        }
    }

    uint64_t alloc_range(const uint64_t page_count) {
        uint64_t index = RANGE_ALLOC / 4096ul;

        for (auto it = regions.begin(); it != stl::LinkedList<Region>::end(); ++it) {
            if (!it->used && it->size >= page_count) {
                alloc_from_region(it, page_count);
                return index;
            }

            index += it->size;
        }

        return 0;
    }

    void free_range(const uint64_t first_page) {
        uint64_t index = RANGE_ALLOC / 4096ul;

        for (auto it = regions.begin(); it != stl::LinkedList<Region>::end(); ++it) {
            if (it->used && index == first_page) {
                it->used = false;
                merge(it);

                return;
            }

            index += it->size;
        }

        ERROR("Double free detected");
    }
} // namespace cosmos::memory::virt
