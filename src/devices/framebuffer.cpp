#include "framebuffer.hpp"

#include "limine.hpp"
#include "memory/offsets.hpp"
#include "utils.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::framebuffer {
    uint64_t fb_size() {
        const auto fb = limine::get_framebuffer();
        return fb.height * fb.pitch * 4;
    }

    uint64_t fb_seek(const stl::Rc<vfs::File>& file, const vfs::SeekType type, const int64_t offset) {
        file->seek(fb_size(), type, offset);
        return file->cursor;
    }

    uint64_t fb_read(const stl::Rc<vfs::File>& file, void* buffer, const uint64_t length) {
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

    uint64_t fb_write(const stl::Rc<vfs::File>& file, const void* buffer, const uint64_t length) {
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

    uint64_t fb_ioctl([[maybe_unused]] const stl::Rc<vfs::File>& file, const uint64_t op, [[maybe_unused]] uint64_t arg) {
        switch (op) {
        case IOCTL_GET_INFO: {
            const auto fb = limine::get_framebuffer();

            const auto width = static_cast<uint64_t>(fb.width & 0xFFFF);
            const auto height = static_cast<uint64_t>(fb.height & 0xFFFF);
            const auto pitch = static_cast<uint64_t>(fb.pitch & 0xFFFF);

            return width | (height << 16) | (pitch << 32);
        }

        default:
            return vfs::IOCTL_UNKNOWN;
        }
    }

    static constexpr vfs::FileOps fb_ops = {
        .seek = fb_seek,
        .read = fb_read,
        .write = fb_write,
        .ioctl = fb_ioctl,
    };

    void init(vfs::Node* node) {
        vfs::devfs::register_device(node, "framebuffer", &fb_ops, nullptr);
    }
} // namespace cosmos::devices::framebuffer
