#include "devfs.hpp"

#include "log.hpp"
#include "utils.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::log {
    uint64_t log_seek(const stl::Rc<vfs::File>& file, const vfs::SeekType type, const int64_t offset) {
        file->seek(get_size(), type, offset);
        return file->cursor;
    }

    uint64_t log_read(const stl::Rc<vfs::File>& file, void* buffer, const uint64_t length) {
        if (file->cursor >= get_size()) return 0;

        auto size = get_size() - file->cursor;
        if (size > length) size = length;

        if (size > 0) {
            utils::memcpy(buffer, &get_start()[file->cursor], size);
            file->cursor += size;
        }

        return size;
    }

    uint64_t log_ioctl([[maybe_unused]] const stl::Rc<vfs::File>& file, [[maybe_unused]] uint64_t op, [[maybe_unused]] uint64_t arg) {
        return vfs::IOCTL_UNKNOWN;
    }

    static constexpr vfs::FileOps log_ops = {
        .seek = log_seek,
        .read = log_read,
        .write = nullptr,
        .ioctl = log_ioctl,
    };

    void init_devfs(vfs::Node* node) {
        vfs::devfs::register_device(node, "log", &log_ops, nullptr);
    }
} // namespace cosmos::log
