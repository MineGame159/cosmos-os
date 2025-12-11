#include "info.hpp"

#include "memory/physical.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::info {
    // meminfo

    void meminfo_reset(vfs::devfs::Sequence* seq) {
        seq->index = 0;
    }

    void meminfo_next(vfs::devfs::Sequence* seq) {
        seq->index++;
        if (seq->index >= 3) seq->index = -1;
    }

    void meminfo_show(vfs::devfs::Sequence* seq) {
        switch (seq->index) {
        case 0:
            seq->printf("total_pages: %llu\n", memory::phys::get_total_pages());
            break;

        case 1:
            seq->printf("used_pages: %llu\n", memory::phys::get_used_pages());
            break;

        case 2:
            seq->printf("free_pages: %llu\n", memory::phys::get_free_pages());
            break;

        default:
            seq->printf("<invalid_index>\n");
            break;
        }
    }

    static constexpr vfs::devfs::SequenceOps meminfo_ops = {
        .reset = meminfo_reset,
        .next = meminfo_next,
        .show = meminfo_show,
    };

    // Init

    void init(vfs::Node* node) {
        vfs::devfs::register_sequence_device(node, "meminfo", &meminfo_ops);
    }
} // namespace cosmos::devices::info
