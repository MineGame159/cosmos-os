#include "atapio.hpp"

#include "log/log.hpp"
#include "memory/heap.hpp"
#include "stl/bit_field.hpp"
#include "utils.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::atapio {
    constexpr uint16_t PRIMARY_BUS_IO = 0x1F0;
    constexpr uint16_t PRIMARY_BUS_CTRL = 0x3F6;

    constexpr uint16_t SECONDARY_BUS_IO = 0x170;
    constexpr uint16_t SECONDARY_BUS_CTRL = 0x376;

    constexpr uint16_t IO_DATA = 0;
    constexpr uint16_t IO_ERROR = 1;
    constexpr uint16_t IO_FEATURES = 1;
    constexpr uint16_t IO_SECTOR_COUNT = 2;
    constexpr uint16_t IO_LBA_LOW = 3;
    constexpr uint16_t IO_LBA_MID = 4;
    constexpr uint16_t IO_LBA_HIGH = 5;
    constexpr uint16_t IO_DRIVE_HEAD = 6;
    constexpr uint16_t IO_STATUS = 7;
    constexpr uint16_t IO_COMMAND = 7;

    constexpr uint16_t CTRL_ALTERNATE_STATUS = 0;
    constexpr uint16_t CTRL_DEVICE_CONTROL = 0;
    constexpr uint16_t CTRL_DRIVE_ADDRESS = 1;

    enum class Error : uint8_t {
        AddressMarkNotFound = 1 << 0,
        TrackZeroNotFound = 1 << 1,
        AbortedCommand = 1 << 2,
        MediaChangeRequest = 1 << 3,
        IdNotFound = 1 << 4,
        MediaChanged = 1 << 5,
        UncorrectableData = 1 << 6,
        BadBlock = 1 << 7,
    };
    ENUM_BIT_FIELD(Error)

    struct DriveHead {
        uint8_t raw;

        DriveHead() {
            raw = 0b10100000;
        }

        FIELD_BITS(block_number, raw, 0, 0b1111)
        FIELD_BIT(use_slave_drive, raw, 4)
        FIELD_BIT(use_lba, raw, 6)
    };

    enum class Status : uint8_t {
        Error = 1 << 0,
        Index = 1 << 1,
        CorrectedData = 1 << 2,
        DRQ = 1 << 3,
        OverlappedModeServiceRequest = 1 << 4,
        DriveFaultError = 1 << 5,
        Ready = 1 << 6,
        Busy = 1 << 7,
    };
    ENUM_BIT_FIELD(Status)

    enum class DeviceCtrl : uint8_t {
        DisableInterrupts = 1 << 1,
        SoftwareReset = 1 << 2,
        HighOrderByte = 1 << 7,
    };
    ENUM_BIT_FIELD(DeviceCtrl)

    struct DriveAddress {
        uint8_t raw;

        FIELD_BIT(drive0, raw, 0)
        FIELD_BIT(drive1, raw, 1)
        FIELD_BITS(selected_head, raw, 2, 0b1111)
        FIELD_BIT(write_gate, raw, 6)
    };

    static uint8_t bus_primary_drive_head = 0;
    static uint8_t bus_secondary_drive_head = 0;

    void write_io(const bool bus_primary, const uint16_t port, const uint8_t data) {
        utils::byte_out((bus_primary ? PRIMARY_BUS_IO : SECONDARY_BUS_IO) + port, data);
    }

    template <typename T>
    T read_io(const bool bus_primary, const uint16_t port) {
        static_assert(sizeof(T) == 1 || sizeof(T) == 2, "Can only read 8 or 16 bit values");

        if (sizeof(T) == 8) {
            return static_cast<T>(utils::byte_in((bus_primary ? PRIMARY_BUS_IO : SECONDARY_BUS_IO) + port));
        }

        return static_cast<T>(utils::short_in((bus_primary ? PRIMARY_BUS_IO : SECONDARY_BUS_IO) + port));
    }

    void select_drive(const bool bus_primary, const DriveHead reg) {
        auto& drive_head = bus_primary ? bus_primary_drive_head : bus_secondary_drive_head;
        if (drive_head == reg.raw) return;

        write_io(bus_primary, IO_DRIVE_HEAD, reg.raw);
        drive_head = reg.raw;

        for (auto i = 0; i < 15; i++) {
            utils::wait();
        }
    }

    void select_drive(const bool bus_primary, const bool drive_slave) {
        DriveHead reg;
        reg.use_slave_drive(drive_slave);

        select_drive(bus_primary, reg);
    }

    // VFS Device

    struct Drive {
        bool bus_primary;
        bool slave;

        bool lba48;
        uint32_t lba28_count;
        uint64_t lba48_count;

        [[nodiscard]]
        uint64_t size() const {
            return (lba48 ? static_cast<uint64_t>(lba48_count) : lba28_count) * 512ull;
        }
    };

    uint64_t seek(vfs::File* file, const vfs::SeekType type, const int64_t offset) {
        const auto drive = static_cast<Drive*>(file->node->fs_handle);

        file->seek(drive->size(), type, offset);
        return file->cursor;
    }

    uint64_t read(vfs::File* file, void* buffer, uint64_t length) {
        const auto drive = static_cast<Drive*>(file->node->fs_handle);

        // Calculate LBA and sector count
        auto sectors = (length + file->cursor % 512 + 511) / 512;
        const auto lba = file->cursor / 512;

        if (drive->lba48 && sectors > 0xFFFF) {
            sectors = 0xFFFF;
            length = 0xFFFF * 512;
        } else if (!drive->lba48 && length > 0xFF) {
            sectors = 0xFF;
            length = 0xFF * 512;
        }

        if (drive->lba48 && lba + sectors > 0xFFFF) {
            sectors = lba + sectors - 0xFFFF;
            length = static_cast<int64_t>(sectors & 0xFFFF) * 512;
        } else if (!drive->lba48 && lba + sectors > 0xFF) {
            sectors = lba + sectors - 0xFF;
            length = static_cast<int64_t>(sectors & 0xFF) * 512;
        }

        if (sectors == 0) return 0;

        // Select drive
        DriveHead reg;
        reg.use_slave_drive(drive->slave);
        reg.use_lba(true);

        if (!drive->lba48) {
            reg.block_number((lba >> 24) & 0b1111);
        }

        select_drive(drive->bus_primary, reg);

        // Read
        if (drive->lba48) {
            write_io(drive->bus_primary, IO_SECTOR_COUNT, (sectors >> 8) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_LOW, (lba >> 24) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_MID, (lba >> 32) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_HIGH, (lba >> 40) & 0xFF);

            write_io(drive->bus_primary, IO_SECTOR_COUNT, (sectors >> 0) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_LOW, (lba >> 0) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_MID, (lba >> 8) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_HIGH, (lba >> 16) & 0xFF);

            write_io(drive->bus_primary, IO_COMMAND, 0x24);
        } else {
            write_io(drive->bus_primary, IO_SECTOR_COUNT, (sectors >> 0) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_LOW, (lba >> 0) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_MID, (lba >> 8) & 0xFF);
            write_io(drive->bus_primary, IO_LBA_HIGH, (lba >> 16) & 0xFF);

            write_io(drive->bus_primary, IO_COMMAND, 0x20);
        }

        // Read data
        auto to_skip = file->cursor - lba * 512;
        auto dst = static_cast<uint8_t*>(buffer);

        const auto read = length;
        file->cursor += length;

        while (sectors > 0) {
            for (auto i = 0; i < 4; i++) {
                read_io<Status>(drive->bus_primary, IO_STATUS);
            }

            auto status = read_io<Status>(drive->bus_primary, IO_STATUS);

            while (status / Status::Busy || !(status / Status::DRQ)) {
                status = read_io<Status>(drive->bus_primary, IO_STATUS);
            }

            for (auto i = 0; i < 256; i++) {
                auto data = read_io<uint16_t>(drive->bus_primary, IO_DATA);

                if (to_skip > 0) {
                    data = data >> 8;
                    to_skip--;
                } else if (length > 0) {
                    *(dst++) = data & 0xFF;
                    data = data >> 8;
                    length--;
                }

                if (to_skip > 0) {
                    to_skip--;
                } else if (length > 0) {
                    *(dst++) = data;
                    length--;
                }
            }

            for (auto i = 0; i < 15; i++) {
                utils::wait();
            }

            sectors--;
        }

        return read;
    }

    uint64_t ioctl([[maybe_unused]] vfs::File* file, [[maybe_unused]] uint64_t op, [[maybe_unused]] uint64_t arg) {
        return vfs::IOCTL_UNKNOWN;
    }

    static constexpr vfs::FileOps ops = {
        .seek = seek,
        .read = read,
        .write = nullptr,
        .ioctl = ioctl,
    };

    // Init

    void identify(vfs::Node* node, const bool bus_primary, const bool drive_slave) {
        // Select drive
        select_drive(bus_primary, drive_slave);

        // Send identify command
        write_io(bus_primary, IO_LBA_LOW, 0);
        write_io(bus_primary, IO_LBA_MID, 0);
        write_io(bus_primary, IO_LBA_HIGH, 0);
        write_io(bus_primary, IO_COMMAND, 0xEC);

        if (read_io<uint8_t>(bus_primary, IO_STATUS) == 0) return;

        // Wait for device to not be busy
        while (read_io<Status>(bus_primary, IO_STATUS) / Status::Busy) {
            utils::wait();
        }

        // If LBA_MID or LBA_HIGH are not 0 then the drive is not ATA
        const auto lba_mid = read_io<uint8_t>(bus_primary, IO_LBA_MID);
        const auto lba_high = read_io<uint8_t>(bus_primary, IO_LBA_HIGH);
        if (lba_mid != 0 || lba_high != 0) return;

        // Wait for either DRQ or Error status
        auto status = read_io<Status>(bus_primary, IO_STATUS);

        while (!(status / Status::DRQ || status / Status::Error)) {
            status = read_io<Status>(bus_primary, IO_STATUS);
        }

        if (status / Status::Error) return;

        // Create drive
        const auto drive = memory::heap::alloc<Drive>();

        drive->bus_primary = bus_primary;
        drive->slave = drive_slave;
        drive->lba48 = false;
        drive->lba28_count = 0;
        drive->lba48_count = 0;

        // Read data
        for (auto i = 0; i < 256; i++) {
            const auto data = read_io<uint16_t>(bus_primary, IO_DATA);

            if (i == 83 && (data & (1u << 10u))) {
                drive->lba48 = true;
            } else if (i == 60) {
                drive->lba28_count = drive->lba28_count | (static_cast<uint32_t>(data) << 0);
            } else if (i == 61) {
                drive->lba28_count = drive->lba28_count | (static_cast<uint32_t>(data) << 16);
            } else if (i == 100) {
                drive->lba48_count = drive->lba48_count | (static_cast<uint64_t>(data) << 0);
            } else if (i == 101) {
                drive->lba48_count = drive->lba48_count | (static_cast<uint64_t>(data) << 16);
            } else if (i == 102) {
                drive->lba48_count = drive->lba48_count | (static_cast<uint64_t>(data) << 32);
            } else if (i == 103) {
                drive->lba48_count = drive->lba48_count | (static_cast<uint64_t>(data) << 48);
            }
        }

        // Create VFS device
        char name[6];
        name[0] = 'a';
        name[1] = 't';
        name[2] = 'a';
        name[3] = bus_primary ? '1' : '0';
        name[4] = drive_slave ? '0' : '1';
        name[5] = '\0';

        vfs::devfs::register_device(node, name, &ops, drive);
    }

    void init(vfs::Node* node) {
        if (utils::byte_in(PRIMARY_BUS_IO + IO_STATUS) != 0xFF) {
            identify(node, true, false);
            identify(node, true, true);
        }

        if (utils::byte_in(SECONDARY_BUS_IO + IO_STATUS) != 0xFF) {
            identify(node, false, false);
            identify(node, false, true);
        }
    }
} // namespace cosmos::devices::atapio
