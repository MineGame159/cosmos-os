#include "iso9660.hpp"

#include "log/log.hpp"
#include "memory/heap.hpp"
#include "stl/bit_field.hpp"
#include "stl/utils.hpp"
#include "utils.hpp"
#include "vfs.hpp"

namespace cosmos::vfs::iso9660 {
    enum class DescriptorType : uint8_t {
        BootRecord = 0,
        PrimaryVolume = 1,
        SupplementaryVolume = 2,
        VolumePartition = 3,
        SetTerminator = 255,
    };

    struct [[gnu::packed]] DescriptorHeader {
        DescriptorType type;
        char identifier[5];
        uint8_t version;
    };

    enum class FileFlags : uint8_t {
        Hidden = 1 << 0,
        Directory = 1 << 1,
        AssociatedFile = 1 << 2,
        FormatInfo = 1 << 3,
        Permissions = 1 << 4,
        Split = 1 << 7,
    };
    ENUM_BIT_FIELD(FileFlags)

    struct [[gnu::packed]] DirectoryEntry {
        uint8_t length;
        uint8_t extended_length;
        uint32_t data_lba;
        uint32_t _msb_data_lba;
        uint32_t data_size;
        uint32_t _msb_data_size;
        char date[7];
        FileFlags flags;
        uint8_t unit_size;
        uint8_t gap_size;
        uint16_t volume_sequence_number;
        uint16_t _msb_volume_sequence_number;
        uint8_t identifier_length;
    };

    struct [[gnu::packed]] PrimaryVolumeDescriptor {
        uint8_t _p0[1];
        char system_identifier[32];
        char volume_identifier[32];
        uint8_t _p1[8];
        uint32_t volume_space_size;
        uint32_t _msb_volume_space_size;
        uint8_t _p2[32];
        uint16_t volume_set_size;
        uint16_t _msb_volume_set_size;
        uint16_t volume_sequence_number;
        uint16_t _msb_volume_sequence_number;
        uint16_t logical_block_size;
        uint16_t _msb_logical_block_size;
        uint32_t path_table_size;
        uint32_t _msb_path_table_size;
        uint32_t l_path_table_lba;
        uint32_t optional_l_path_table_lba;
        uint32_t msb_m_path_table_lba;
        uint32_t msb_optional_m_path_table_lba;
        DirectoryEntry root_directory;
        uint8_t _p3[1];
        char volume_set_identifier[128];
        char publisher_identifier[128];
        char data_preparer_identifier[128];
        char application_identifier[128];
        char copyright_file_identifier[37];
        char abstract_file_identifier[37];
        char bibliographic_file_identifier[37];
        char volume_creation_date[17];
        char volume_modification_date[17];
        char volume_expiration_date[17];
        char volume_effective_date[17];
        uint8_t file_structure_version;
        uint8_t _p4[1];
        uint8_t application[512];
    };

    // SUSP (System Use Sharing Protocol)

    struct [[gnu::packed]] SuspHeader {
        char signature[2];
        uint8_t length;
        uint8_t version;
    };

    struct SuspTagIterator {
        DirectoryEntry* entry;
        uint32_t offset;

        SuspHeader* header;

        bool next() {
            if (offset + 4 > entry->length) return false;

            header = reinterpret_cast<SuspHeader*>(reinterpret_cast<uint8_t*>(entry) + offset);
            if (header->length < 4 || offset + header->length > entry->length) return false;

            offset += header->length;
            return true;
        }

        template <typename T>
        T* check(const char sig0, const char sig1) {
            if (header->signature[0] == sig0 && header->signature[1] == sig1) return reinterpret_cast<T*>(header);
            return nullptr;
        }
    };

    SuspTagIterator iterate_susp_tags(DirectoryEntry* entry) {
        SuspTagIterator it;
        it.entry = entry;
        it.header = nullptr;

        it.offset = sizeof(DirectoryEntry) + entry->identifier_length;
        if (it.offset % 2 == 1) it.offset++;

        return it;
    }

    struct [[gnu::packed]] SuspSp {
        SuspHeader header;
        uint8_t check_bytes[2];
        uint8_t skip_length;
    };

    // RRIP (Rock Ridge Interchange Protocol)

    enum class RripNmFlags : uint8_t {
        Continue = 1 << 0,
        Current = 1 << 1,
        Parent = 1 << 2,
        Host = 1 << 5,
    };
    ENUM_BIT_FIELD(RripNmFlags)

    struct [[gnu::packed]] RripNm {
        SuspHeader header;
        RripNmFlags flags;
    };

    // Info

    struct FsInfo {
        File* device;
        uint64_t block_size;
        bool uses_susp;
    };

    struct NodeInfo {
        uint64_t data_offset;
        uint64_t data_size;
    };

    // FileOps

    uint64_t file_seek(File* file, const SeekType type, const int64_t offset) {
        const auto node_info = reinterpret_cast<NodeInfo*>(file->node + 1);
        file->seek(node_info->data_size, type, offset);
        return file->cursor;
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    uint64_t file_read(File* file, void* buffer, const uint64_t length) {
        const auto fs_info = static_cast<FsInfo*>(file->node->fs_handle);
        const auto node_info = reinterpret_cast<NodeInfo*>(file->node + 1);

        fs_info->device->ops->seek(fs_info->device, SeekType::Start, static_cast<int64_t>(node_info->data_offset + file->cursor));

        const auto to_read = stl::min(length, node_info->data_size - file->cursor);
        const auto read = fs_info->device->ops->read(fs_info->device, buffer, to_read);

        file->cursor += read;
        return read;
    }

    uint64_t file_ioctl([[maybe_unused]] File* file, [[maybe_unused]] uint64_t op, [[maybe_unused]] uint64_t arg) {
        return IOCTL_UNKNOWN;
    }

    static constexpr FileOps file_ops = {
        .seek = file_seek,
        .read = file_read,
        .write = nullptr,
        .ioctl = file_ioctl,
    };

    // FsOps

    Node* fs_create([[maybe_unused]] Node* parent, [[maybe_unused]] NodeType type, [[maybe_unused]] stl::StringView name) {
        return nullptr;
    }

    bool fs_destroy([[maybe_unused]] Node* node) {
        return false;
    }

    void fs_populate(Node* node) {
        const auto fs_info = static_cast<FsInfo*>(node->fs_handle);
        const auto node_info = reinterpret_cast<NodeInfo*>(node + 1);

        // Read ISO directory entries
        const auto entries = static_cast<uint8_t*>(memory::heap::alloc(node_info->data_size));

        fs_info->device->ops->seek(fs_info->device, SeekType::Start, static_cast<int64_t>(node_info->data_offset));

        if (fs_info->device->ops->read(fs_info->device, entries, node_info->data_size) != node_info->data_size) {
            ERROR("Failed to read directory entries of '%s'", node->name.data());
            memory::heap::free(entries);
            return;
        }

        // Add child nodes
        auto entry_index = 0u;

        for (auto i = 0u; i + sizeof(DirectoryEntry) < node_info->data_size;) {
            const auto entry = reinterpret_cast<DirectoryEntry*>(&entries[i]);

            if (entry->length == 0) {
                i = stl::align_up(i + 1, 2048u);
                continue;
            }

            if (entry->length < sizeof(DirectoryEntry)) break;

            if (entry_index == 0 && !fs_info->uses_susp && node->mount_root) {
                // Check for SUSP
                auto it = iterate_susp_tags(entry);

                while (it.next()) {
                    if (const auto sp = it.check<SuspSp>('S', 'P')) {
                        fs_info->uses_susp = sp->check_bytes[0] == 0xBE && sp->check_bytes[1] == 0xEF;
                        break;
                    }
                }
            } else if (entry_index >= 2 && !(entry->flags / FileFlags::Hidden)) {
                // Get name from identifier
                auto name = stl::StringView(reinterpret_cast<char*>(&entries[i + sizeof(DirectoryEntry)]), entry->identifier_length);

                const auto semicolonIndex = name.index_of(';');
                if (semicolonIndex != -1) name = name.substr(0, semicolonIndex);

                // Scan for Rock Ridge NM tag
                if (fs_info->uses_susp) {
                    auto it = iterate_susp_tags(entry);

                    while (it.next()) {
                        if (const auto nm = it.check<RripNm>('N', 'M')) {
                            if (nm->flags / RripNmFlags::Current) {
                                name = ".";
                            } else if (nm->flags / RripNmFlags::Parent) {
                                name = "..";
                            } else {
                                name = stl::StringView(reinterpret_cast<const char*>(nm + 1), it.header->length - sizeof(RripNm));
                            }

                            break;
                        }
                    }
                }

                // Create child
                const auto child = node->children.push_back_alloc(sizeof(NodeInfo) + name.size() + 1);

                child->parent = node;
                child->mount_root = false;
                child->type = (entry->flags / FileFlags::Directory) ? NodeType::Directory : NodeType::File;
                child->name = stl::StringView(reinterpret_cast<char*>(child + 1) + sizeof(NodeInfo), name.size());
                child->fs_ops = node->fs_ops;
                child->fs_handle = node->fs_handle;
                child->open_read = 0;
                child->open_write = 0;
                child->populated = false;
                child->children = {};

                utils::memcpy(const_cast<char*>(child->name.data()), name.data(), name.size());
                const_cast<char*>(child->name.data())[name.size()] = '\0';

                // Fill node info
                const auto child_node_info = reinterpret_cast<NodeInfo*>(child + 1);
                child_node_info->data_offset = (entry->data_lba + entry->extended_length) * fs_info->block_size;
                child_node_info->data_size = entry->data_size;
            }

            entry_index++;

            i += entry->length;
            if (i % 2 == 1) i++;
        }

        memory::heap::free(entries);
        node->populated = true;
    }

    const FileOps* fs_open([[maybe_unused]] const Node* node, const Mode mode) {
        if (is_write(mode)) return nullptr;
        return &file_ops;
    }

    void fs_on_close([[maybe_unused]] const File* file) {}

    static constexpr FsOps fs_ops = {
        .create = fs_create,
        .destroy = fs_destroy,
        .populate = fs_populate,
        .open = fs_open,
        .on_close = fs_on_close,
    };

    // Init

    bool init(Node* node, const stl::StringView device_path) {
        // Open device
        const auto device = open(device_path, Mode::Read);
        if (device == nullptr) return false;

        // Find PVD
        const auto descriptor = memory::heap::alloc(2048);
        device->ops->seek(device, SeekType::Start, 16 * 2048);

        for (;;) {
            if (device->ops->read(device, descriptor, 2048) != 2048) {
                ERROR("Failed to read descriptor");
                memory::heap::free(descriptor);
                return false;
            }

            const auto type = static_cast<DescriptorHeader*>(descriptor)->type;
            if (type == DescriptorType::PrimaryVolume) break;

            if (type == DescriptorType::SetTerminator) {
                ERROR("Failed to find Primary Volume Descriptor");
                memory::heap::free(descriptor);
                return false;
            }
        }

        const auto pvd = reinterpret_cast<PrimaryVolumeDescriptor*>(static_cast<DescriptorHeader*>(descriptor) + 1);

        // Create filesystem info
        const auto fs_info = memory::heap::alloc<FsInfo>();
        fs_info->device = device;
        fs_info->block_size = pvd->logical_block_size;
        fs_info->uses_susp = false;

        node->fs_ops = &fs_ops;
        node->fs_handle = fs_info;

        // Create node info
        const auto node_info = reinterpret_cast<NodeInfo*>(node + 1);
        node_info->data_offset = (pvd->root_directory.data_lba + pvd->root_directory.extended_length) * fs_info->block_size;
        node_info->data_size = pvd->root_directory.data_size;

        memory::heap::free(descriptor);
        return true;
    }

    void register_filesystem() {
        vfs::register_filesystem("iso9660", sizeof(NodeInfo), init);
    }
} // namespace cosmos::vfs::iso9660
