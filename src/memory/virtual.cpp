#include "virtual.hpp"

#include "limine.hpp"
#include "log/log.hpp"
#include "offsets.hpp"
#include "physical.hpp"
#include "utils.hpp"

namespace cosmos::memory::virt {
    // Virtual address

    constexpr uint64_t VIRT_ADDR_OFFSET_OFFSET = 0;
    constexpr uint64_t VIRT_ADDR_OFFSET_MASK = 0b111111111111;

    constexpr uint64_t VIRT_ADDR_PT_OFFSET = VIRT_ADDR_OFFSET_OFFSET + 12;
    constexpr uint64_t VIRT_ADDR_PT_MASK = 0b111111111;

    constexpr uint64_t VIRT_ADDR_PD_OFFSET = VIRT_ADDR_PT_OFFSET + 9;
    constexpr uint64_t VIRT_ADDR_PD_MASK = 0b111111111;

    constexpr uint64_t VIRT_ADDR_PDP_OFFSET = VIRT_ADDR_PD_OFFSET + 9;
    constexpr uint64_t VIRT_ADDR_PDP_MASK = 0b111111111;

    constexpr uint64_t VIRT_ADDR_PML4_OFFSET = VIRT_ADDR_PDP_OFFSET + 9;
    constexpr uint64_t VIRT_ADDR_PML4_MASK = 0b111111111;

    constexpr uint64_t VIRT_ADDR_LAST_BIT_OFFSET = VIRT_ADDR_PML4_OFFSET + 9;
    constexpr uint64_t VIRT_ADDR_UNUSED_MASK = 0b1111111111111111;

    Address unpack(const uint64_t virt) {
        return {
            .pml4 = static_cast<uint16_t>((virt >> VIRT_ADDR_PML4_OFFSET) & VIRT_ADDR_PML4_MASK),
            .pdp = static_cast<uint16_t>((virt >> VIRT_ADDR_PDP_OFFSET) & VIRT_ADDR_PDP_MASK),
            .pd = static_cast<uint16_t>((virt >> VIRT_ADDR_PD_OFFSET) & VIRT_ADDR_PD_MASK),
            .pt = static_cast<uint16_t>((virt >> VIRT_ADDR_PT_OFFSET) & VIRT_ADDR_PT_MASK),
            .offset = static_cast<uint16_t>((virt >> VIRT_ADDR_OFFSET_OFFSET) & VIRT_ADDR_OFFSET_MASK),
        };
    }

    uint64_t make_canonical(const uint64_t addr) {
        if (((addr >> (VIRT_ADDR_LAST_BIT_OFFSET - 1)) & 1) == 1) {
            return addr | (VIRT_ADDR_UNUSED_MASK << VIRT_ADDR_LAST_BIT_OFFSET);
        }

        return addr;
    }

    uint64_t pack(const Address addr) {
        uint64_t virt = (addr.pml4 & VIRT_ADDR_PML4_MASK) << VIRT_ADDR_PML4_OFFSET;
        virt |= (addr.pdp & VIRT_ADDR_PDP_MASK) << VIRT_ADDR_PDP_OFFSET;
        virt |= (addr.pd & VIRT_ADDR_PD_MASK) << VIRT_ADDR_PD_OFFSET;
        virt |= (addr.pt & VIRT_ADDR_PT_MASK) << VIRT_ADDR_PT_OFFSET;
        virt |= (addr.offset & VIRT_ADDR_OFFSET_MASK) << VIRT_ADDR_OFFSET_OFFSET;

        return make_canonical(virt);
    }

    // Table entry

    constexpr uint64_t FLAG_PRESENT = 1ul << 0;
    constexpr uint64_t FLAG_WRITABLE = 1ul << 1;
    constexpr uint64_t FLAG_USER = 1ul << 2;
    constexpr uint64_t FLAG_WRITE_THROUGH = 1ul << 3;
    constexpr uint64_t FLAG_CACHE_DISABLE = 1ul << 4;
    constexpr uint64_t FLAG_ACCESSED = 1ul << 5;
    constexpr uint64_t FLAG_DIRECT = 1ul << 7;
    constexpr uint64_t FLAG_NO_EXECUTE = 1ul << 63;

    constexpr uint64_t ADDRESS_MASK /*************/ = 0b00000000'00000111'11111111'11111111'11111111'11111111'11110000'00000000;
    constexpr uint64_t DIRECT_PD_ADDRESS_MASK /***/ = 0b00000000'00000111'11111111'11111111'11111111'11100000'00000000'00000000;
    constexpr uint64_t DIRECT_PDP_ADDRESS_MASK /**/ = 0b00000000'00000111'11111111'11111111'11000000'00000000'00000000'00000000;

    bool entry_is_present(const uint64_t entry) {
        return (entry & FLAG_PRESENT) == FLAG_PRESENT;
    }

    bool entry_is_writable(const uint64_t entry) {
        return (entry & FLAG_WRITABLE) == FLAG_WRITABLE;
    }

    bool entry_is_user(const uint64_t entry) {
        return (entry & FLAG_USER) == FLAG_USER;
    }

    bool entry_is_write_through(const uint64_t entry) {
        return (entry & FLAG_WRITE_THROUGH) == FLAG_WRITE_THROUGH;
    }

    bool entry_is_cache_disabled(const uint64_t entry) {
        return (entry & FLAG_CACHE_DISABLE) == FLAG_CACHE_DISABLE;
    }

    bool entry_is_accessed(const uint64_t entry) {
        return (entry & FLAG_ACCESSED) == FLAG_ACCESSED;
    }

    bool entry_is_direct(const uint64_t entry) {
        return (entry & FLAG_DIRECT) == FLAG_DIRECT;
    }

    bool entry_is_no_execute(const uint64_t entry) {
        return (entry & FLAG_NO_EXECUTE) == FLAG_NO_EXECUTE;
    }

    // Space

    static bool first_create = true;
    static bool gb_pages_supported = false;

    static bool switched_to_space = false;
    static uint64_t kernel_first_pml4_entry = 0;
    static uint64_t kernel_last_pml4_entry = 0;

    template <typename T>
    T* get_ptr_from_phys(const uint64_t phys) {
        if (switched_to_space) return reinterpret_cast<T*>(DIRECT_MAP + phys);
        return reinterpret_cast<T*>(limine::get_hhdm() + phys);
    }

    bool map_kernel(const Space space) {
        for (auto i = 0u; i < limine::get_memory_range_count(); i++) {
            const auto [type, first_page, page_count] = limine::get_memory_range(i);

            if (type == limine::MemoryType::ExecutableAndModules) {
                constexpr auto virt = KERNEL / 4096ul;
                return map_pages(space, virt, first_page, page_count, Flags::Write | Flags::Execute);
            }
        }

        ERROR("Failed to find kernel memory range");
        return false;
    }

    bool map_framebuffer(const Space space) {
        for (auto i = 0u; i < limine::get_memory_range_count(); i++) {
            const auto [type, first_page, page_count] = limine::get_memory_range(i);

            if (type == limine::MemoryType::Framebuffer) {
                constexpr auto virt = FRAMEBUFFER / 4096ul;
                return map_pages(space, virt, first_page, page_count, Flags::Write | Flags::Uncached);
            }
        }

        ERROR("Failed to find framebuffer memory range");
        return false;
    }

    bool map_direct_map(const Space space) {
        constexpr auto virt = DIRECT_MAP / 4096ul;

        for (auto i = 0u; i < limine::get_memory_range_count(); i++) {
            const auto [type, first_page, page_count] = limine::get_memory_range(i);

            if (limine::memory_type_ram(type)) {
                if (!map_pages(space, virt + first_page, first_page, page_count, Flags::Write)) return false;
            }
        }

        return true;
    }

    Space create() {
        if (first_create) {
            uint32_t eax, ebx, ecx, edx;
            utils::cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
            gb_pages_supported = (edx >> 26) & 1;

            first_create = false;
        }

        const auto space = phys::alloc_pages(1);

        if (space == 0) {
            ERROR("Failed to allocate physical page for PML4 table");
            return 0;
        }

        const auto pml4 = get_ptr_from_phys<uint64_t>(space);
        utils::memset(pml4, 0, 4096);

#define MAP(func)                                                                                                                          \
    if (!func(space)) {                                                                                                                    \
        destroy(space);                                                                                                                    \
        return 0;                                                                                                                          \
    }

        if (!switched_to_space) {
            MAP(map_kernel)
            MAP(map_framebuffer)
            MAP(map_direct_map)

            kernel_first_pml4_entry = pml4[256];
            kernel_last_pml4_entry = pml4[511];
        } else {
            pml4[256] = kernel_first_pml4_entry;
            pml4[511] = kernel_last_pml4_entry;
        }

#undef MAP

        return space;
    }

    Space get_current() {
        Space space;
        asm volatile("mov %%cr3, %0" : "=r"(space));

        return space;
    }

    void destroy(const Space space) {
        const auto pml4_table = get_ptr_from_phys<uint64_t>(space);

        for (auto pml4_i = 0; pml4_i < 256; pml4_i++) {
            const auto pml4_entry = pml4_table[pml4_i];
            if (!entry_is_present(pml4_entry)) continue;
            const auto pdp_table = get_ptr_from_phys<uint64_t>(pml4_entry & ADDRESS_MASK);

            for (auto pdp_i = 0; pdp_i < 512; pdp_i++) {
                const auto pdp_entry = pdp_table[pdp_i];
                if (!entry_is_present(pdp_entry)) continue;

                if (entry_is_direct(pdp_entry)) {
                    phys::free_pages((pdp_entry & DIRECT_PDP_ADDRESS_MASK) / 4096ul, 512 * 512);
                    continue;
                }

                const auto pd_table = get_ptr_from_phys<uint64_t>(pdp_entry & ADDRESS_MASK);

                for (auto pd_i = 0; pd_i < 512; pd_i++) {
                    const auto pd_entry = pd_table[pd_i];
                    if (!entry_is_present(pd_entry)) continue;

                    if (entry_is_direct(pd_entry)) {
                        phys::free_pages((pd_entry & DIRECT_PD_ADDRESS_MASK) / 4096ul, 512);
                        continue;
                    }

                    const auto pt_table = get_ptr_from_phys<uint64_t>(pd_entry & ADDRESS_MASK);

                    for (auto pt_i = 0; pt_i < 512; pt_i++) {
                        const auto pt_entry = pt_table[pt_i];
                        if (!entry_is_present(pt_entry)) continue;

                        phys::free_pages((pt_entry & ADDRESS_MASK) / 4096ul, 1);
                    }

                    phys::free_pages((pd_entry & ADDRESS_MASK) / 4096ul, 1);
                }

                phys::free_pages((pdp_entry & ADDRESS_MASK) / 4096ul, 1);
            }

            phys::free_pages((pml4_entry & ADDRESS_MASK) / 4096ul, 1);
        }

        phys::free_pages(space / 4096ul, 1);
    }

    uint64_t* get_child_table(uint64_t& entry) {
        if (!entry_is_present(entry)) {
            const auto child_table_phys = phys::alloc_pages(1);

            if (child_table_phys == 0) {
                ERROR("Failed to allocate physical page for child table");
                return nullptr;
            }

            const auto child_table = get_ptr_from_phys<uint64_t>(child_table_phys);
            utils::memset(child_table, 0, 4096);

            entry = (child_table_phys & ADDRESS_MASK) | FLAG_PRESENT | FLAG_WRITABLE;
        }

        return get_ptr_from_phys<uint64_t>(entry & ADDRESS_MASK);
    }

    bool map_pages(const Space space, uint64_t virt, uint64_t phys, uint64_t count, const Flags flags) {
        // Get entry flags
        auto entry_flags = FLAG_PRESENT;

        if (flags / Flags::Write) entry_flags |= FLAG_WRITABLE;
        if (!(flags / Flags::Execute)) entry_flags |= FLAG_NO_EXECUTE;
        if (flags / Flags::Uncached) entry_flags |= FLAG_CACHE_DISABLE | FLAG_WRITE_THROUGH;

        // Map
        const auto pml4_table = get_ptr_from_phys<uint64_t>(space);
        const auto invalidate = get_current() == space;

        while (count > 0) {
            const auto addr = unpack(virt * 4096);

            const auto pdp_table = get_child_table(pml4_table[addr.pml4]);
            if (pdp_table == nullptr) return false;

            // 1 gB
            if (gb_pages_supported && virt % (512 * 512) == 0 && phys % (512 * 512) == 0 && count >= (512 * 512)) {
                pdp_table[addr.pdp] = ((phys * 4096) & DIRECT_PDP_ADDRESS_MASK) | FLAG_DIRECT | entry_flags;
                if (invalidate) asm volatile("invlpg (%0)" ::"r"(virt * 4096ul) : "memory");

                virt += 512 * 512;
                phys += 512 * 512;
                count -= 512 * 512;

                continue;
            }

            const auto pd_table = get_child_table(pdp_table[addr.pdp]);
            if (pd_table == nullptr) return false;

            // 2 mB
            if (virt % 512 == 0 && phys % 512 == 0 && count >= 512) {
                pd_table[addr.pd] = ((phys * 4096) & DIRECT_PD_ADDRESS_MASK) | FLAG_DIRECT | entry_flags;
                if (invalidate) asm volatile("invlpg (%0)" ::"r"(virt * 4096ul) : "memory");

                virt += 512;
                phys += 512;
                count -= 512;

                continue;
            }

            // 4 kB
            const auto pt_table = get_child_table(pd_table[addr.pd]);
            if (pt_table == nullptr) return false;

            pt_table[addr.pt] = ((phys * 4096) & ADDRESS_MASK) | entry_flags;
            if (invalidate) asm volatile("invlpg (%0)" ::"r"(virt * 4096ul) : "memory");

            virt++;
            phys++;
            count--;
        }

        return true;
    }

    void switch_to(Space space) {
        asm volatile("mov %0, %%cr3" ::"ri"(space));
        switched_to_space = true;
    }

    bool switched() {
        return switched_to_space;
    }

    uint64_t get_phys(const uint64_t virt) {
        const auto [pml4, pdp, pd, pt, offset] = unpack(virt);

        const auto space = get_current();
        const auto pml4_table = get_ptr_from_phys<uint64_t>(space);

        // PML4 Entry - PDP Table
        if (!entry_is_present(pml4_table[pml4])) return 0;
        const auto pdp_table = get_ptr_from_phys<uint64_t>(pml4_table[pml4] & ADDRESS_MASK);

        // PDP Entry - PD Table
        if (!entry_is_present(pdp_table[pdp])) return 0;
        if (entry_is_direct(pdp_table[pdp])) // Direct entry - 1 gB page
            return (pdp_table[pdp] & DIRECT_PDP_ADDRESS_MASK) + ((pd << VIRT_ADDR_PD_OFFSET) | (pt << VIRT_ADDR_PT_OFFSET) | offset);
        const auto pd_table = get_ptr_from_phys<uint64_t>(pdp_table[pdp] & ADDRESS_MASK);

        // PD Entry - PT Table
        if (!entry_is_present(pd_table[pd])) return 0;
        if (entry_is_direct(pd_table[pd])) // Direct entry - 2 mB page
            return (pd_table[pd] & DIRECT_PD_ADDRESS_MASK) + ((pt << VIRT_ADDR_PT_OFFSET) | offset);
        const auto pt_table = get_ptr_from_phys<uint64_t>(pd_table[pd] & ADDRESS_MASK);

        // PT Table - Page
        if (!entry_is_present(pt_table[pt])) return 0;
        const auto page_phys = pt_table[pt] & ADDRESS_MASK;

        return page_phys + offset;
    }

    void dump(const Space space, void (*range_fn)(uint64_t virt_start, uint64_t virt_end)) {
        uint64_t current_start = 0;
        uint64_t current_end = 0;
        bool has_active_range = false;

        auto add_mapping = [&](const uint64_t virt_addr, const uint64_t size) {
            if (has_active_range && current_end == virt_addr) {
                current_end += size;
            } else {
                if (has_active_range) {
                    range_fn(current_start, current_end);
                }

                current_start = virt_addr;
                current_end = virt_addr + size;
                has_active_range = true;
            }
        };

        const auto pml4_table = get_ptr_from_phys<uint64_t>(space);

        for (auto pml4_index = 0; pml4_index < 512; pml4_index++) {
            const auto pml4_entry = pml4_table[pml4_index];
            if (!entry_is_present(pml4_entry)) continue;

            const auto pml4_virt = static_cast<uint64_t>(pml4_index) << VIRT_ADDR_PML4_OFFSET;

            const auto pdp_table = get_ptr_from_phys<uint64_t>(pml4_entry & ADDRESS_MASK);

            for (auto pdp_index = 0; pdp_index < 512; pdp_index++) {
                const auto pdp_entry = pdp_table[pdp_index];
                if (!entry_is_present(pdp_entry)) continue;

                const auto pdp_virt = make_canonical(pml4_virt | (static_cast<uint64_t>(pdp_index) << VIRT_ADDR_PDP_OFFSET));

                if (entry_is_direct(pdp_entry)) {
                    add_mapping(pdp_virt, 1ULL << VIRT_ADDR_PDP_OFFSET); // 1 gB size
                    continue;
                }

                const auto pd_table = get_ptr_from_phys<uint64_t>(pdp_entry & ADDRESS_MASK);

                for (auto pd_index = 0; pd_index < 512; pd_index++) {
                    const auto pd_entry = pd_table[pd_index];
                    if (!entry_is_present(pd_entry)) continue;

                    const auto pd_virt = pdp_virt | (static_cast<uint64_t>(pd_index) << VIRT_ADDR_PD_OFFSET);

                    if (entry_is_direct(pd_entry)) {
                        add_mapping(pd_virt, 1ULL << VIRT_ADDR_PD_OFFSET); // 2 mB size
                        continue;
                    }

                    const auto pt_table = get_ptr_from_phys<uint64_t>(pd_entry & ADDRESS_MASK);

                    for (auto pt_index = 0; pt_index < 512; pt_index++) {
                        const auto pt_entry = pt_table[pt_index];
                        if (!entry_is_present(pt_entry)) continue;

                        const auto pt_virt = pd_virt | (static_cast<uint64_t>(pt_index) << VIRT_ADDR_PT_OFFSET);
                        add_mapping(pt_virt, 4096); // 4 kB size
                    }
                }
            }
        }

        if (has_active_range) {
            range_fn(current_start, current_end);
        }
    }
} // namespace cosmos::memory::virt
