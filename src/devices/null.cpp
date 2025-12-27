#include "null.hpp"

#include "utils.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::null {
    uint64_t seek(const stl::Rc<vfs::File>& file, const vfs::SeekType type, const int64_t offset) {
        file->seek(0xFFFFFFFFFFFFFFFF, type, offset);
        return file->cursor;
    }

    uint64_t read([[maybe_unused]] const stl::Rc<vfs::File>& file, void* buffer, const uint64_t length) {
        utils::memset(buffer, 0, length);
        return length;
    }

    uint64_t write([[maybe_unused]] const stl::Rc<vfs::File>& file, [[maybe_unused]] const void* buffer, const uint64_t length) {
        return length;
    }

    uint64_t ioctl([[maybe_unused]] const stl::Rc<vfs::File>& file, [[maybe_unused]] uint64_t op, [[maybe_unused]] uint64_t arg) {
        return vfs::IOCTL_UNKNOWN;
    }

    static constexpr vfs::FileOps ops = {
        .seek = seek,
        .read = read,
        .write = write,
        .ioctl = ioctl,
    };

    // Header

    void init(vfs::Node* node) {
        vfs::devfs::register_device(node, "null", &ops, nullptr);
    }
} // namespace cosmos::devices::null
