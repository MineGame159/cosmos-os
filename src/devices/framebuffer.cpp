#include "framebuffer.hpp"

#include "limine.hpp"
#include "memory/offsets.hpp"
#include "utils.hpp"
#include "vfs/devfs.hpp"
#include "vfs/vfs.hpp"

namespace cosmos::devices::framebuffer {
    uint64_t fb_size() {
        const auto fb = limine::get_framebuffer();
        return fb.width * fb.height * 4;
    }

    uint64_t fb_seek(vfs::File* file, const vfs::SeekType type, const int64_t offset) {
        file->seek(fb_size(), type, offset);
        return file->cursor;
    }

    uint64_t fb_read(vfs::File* file, void* buffer, const uint64_t length) {
        if (file->mode == vfs::Mode::Write) return 0;
        if (file->cursor >= fb_size()) return 0;

        auto size = fb_size() - file->cursor;
        if (size > length) size = length;

        if (size > 0) {
            utils::memcpy(buffer, &reinterpret_cast<uint8_t*>(memory::virt::FRAMEBUFFER)[file->cursor], size);
            file->cursor += size;
        }

        return size;
    }

    uint64_t fb_write(vfs::File* file, const void* buffer, const uint64_t length) {
        if (file->mode == vfs::Mode::Read) return 0;
        if (file->cursor >= fb_size()) return 0;

        auto size = fb_size() - file->cursor;
        if (size > length) size = length;

        if (size > 0) {
            utils::memcpy(&reinterpret_cast<uint8_t*>(memory::virt::FRAMEBUFFER)[file->cursor], buffer, size);
            file->cursor += size;
        }

        return size;
    }

    static constexpr vfs::FileOps fb_ops = {
        .seek = fb_seek,
        .read = fb_read,
        .write = fb_write,
    };

    void init(void* devfs_handle) {
        vfs::devfs::register_device(devfs_handle, "framebuffer", &fb_ops);
    }
} // namespace cosmos::devices::framebuffer
