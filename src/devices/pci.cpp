#include "pci.hpp"

#include "log/log.hpp"
#include "stl/bit_field.hpp"
#include "utils.hpp"
#include "vfs/devfs.hpp"

namespace cosmos::devices::pci {
    constexpr uint16_t ADDRESS = 0xCF8;
    constexpr uint16_t DATA = 0xCFC;

    struct Address {
        uint32_t raw;

        FIELD_BITS(offset, raw, 0, 0xFF)
        FIELD_BITS(function, raw, 8, 0b111)
        FIELD_BITS(device, raw, 11, 0b11111)
        FIELD_BITS(bus, raw, 16, 0xFF)
        FIELD_BIT(enabled, raw, 31)
    };

    enum class Command : uint16_t {
        IO = 1 << 0,
        Memory = 1 << 1,
        BusMaster = 1 << 2,
        SpecialCycles = 1 << 3,
        MemoryWriteAndInvalidate = 1 << 4,
        VGAPaletteSnoop = 1 << 5,
        ParityErrorResponse = 1 << 6,
        SERR = 1 << 8,
        FastBackToBack = 1 << 9,
        InterruptDisable = 1 << 10,
    };
    ENUM_BIT_FIELD(Command)

    enum class Status : uint16_t {
        Interrupt = 1 << 3,
        Capabilities = 1 << 4,
        MHz66 = 1 << 5,
        FastBackToBack = 1 << 7,
        MasterDataParityError = 1 << 8,
        DevSelTiming1 = 1 << 9,
        DevSelTiming2 = 1 << 10,
        SignaledTargetAbort = 1 << 11,
        ReceivedTargetAbort = 1 << 12,
        ReceivedMasterAbort = 1 << 13,
        SignaledSystemError = 1 << 14,
        DetectedParityError = 1 << 15,
    };
    ENUM_BIT_FIELD(Status)

    struct [[gnu::packed]] Header {
        uint16_t vendor_id;
        uint16_t device_id;

        Command command;
        Status status;

        uint8_t revision_id;
        uint8_t prog_if;
        uint8_t subclass;
        uint8_t class_code;

        uint8_t cache_line_size;
        uint8_t latency_timer;
        uint8_t header_type;
        uint8_t bist;
    };

    Address get_address(const uint8_t bus, const uint8_t device, const uint8_t function) {
        auto address = Address{};

        address.enabled(true);
        address.bus(bus);
        address.device(device);
        address.function(function);

        return address;
    }

    uint16_t read_uint16(const uint8_t bus, const uint8_t device, const uint8_t function, const uint8_t offset) {
        auto address = get_address(bus, device, function);
        address.offset(offset);

        utils::int_out(ADDRESS, address.raw);
        return static_cast<uint16_t>((utils::int_in(DATA) >> ((offset & 2) * 8)) & 0xFFFF);
    }

    template <typename T>
    T read_header(const uint8_t bus, const uint8_t device, const uint8_t function) {
        static_assert(sizeof(T) % 4 == 0, "Header needs to be a multiple of 4 bytes");

        auto address = get_address(bus, device, function);

        T header;
        const auto header_ptr = reinterpret_cast<uint32_t*>(&header);

        for (auto i = 0u; i < sizeof(T) / 4; i++) {
            address.offset(i * 4);

            utils::int_out(ADDRESS, address.raw);
            header_ptr[i] = utils::int_in(DATA);
        }

        return header;
    }

    struct Device {
        uint8_t bus_num;
        uint8_t num;
        uint8_t function_num;

        uint8_t class_code;
        uint8_t subclass;

        uint16_t vendor_id;
        uint16_t device_id;
    };

    static stl::LinkedList<Device> devices = {};

    void check_function(const uint8_t bus, const uint8_t device_num, const uint8_t function) {
        const auto header = read_header<Header>(bus, device_num, function);
        const auto device = devices.push_back_alloc();

        device->bus_num = bus;
        device->num = device_num;
        device->function_num = function;

        device->class_code = header.class_code;
        device->subclass = header.subclass;

        device->vendor_id = header.vendor_id;
        device->device_id = header.device_id;
    }

    void check_device(const uint8_t bus, const uint8_t device) {
        uint8_t function = 0;

        const auto vendor_id = read_uint16(bus, device, function, offsetof(Header, vendor_id));
        if (vendor_id == 0xFFFF) return;

        check_function(bus, device, function);
        const auto header_type = read_uint16(bus, device, function, offsetof(Header, header_type));

        if ((header_type & 0x80) != 0) {
            for (function = 1; function < 8; function++) {
                if (read_uint16(bus, device, function, offsetof(Header, vendor_id)) != 0xFFFF) {
                    check_function(bus, device, function);
                }
            }
        }
    }

    // pci_devices file

    template <std::size_t N>
    struct FixedString {
        char content[N]{};

        constexpr FixedString(const char (&input)[N]) { // NOLINT(*-explicit-constructor)
            for (auto i = 0u; i < N; i++) {
                content[i] = input[i];
            }
        }
    };

    struct ClassInfo {
        const char* name;
        size_t subclass_count;
        const char* const* subclass_names;

        [[nodiscard]]
        const char* get_subclass_name(const uint8_t subclass) const {
            return subclass < subclass_count ? subclass_names[subclass] : "Unknown";
        }
    };

    template <FixedString Name, FixedString... SubClasses>
    constexpr ClassInfo parse_info() {
        static constexpr const char* subs[] = { SubClasses.content..., nullptr };
        return ClassInfo{ Name.content, sizeof...(SubClasses), subs };
    }

    // Some subclasses are skipping because they aren't continuous
    constexpr ClassInfo CLASSES[] = {
        parse_info<"Unclassified", "Non-VGA-Compatible Unclassified Device", "VGA-Compatible Unclassified Device">(),
        parse_info<"Mass Storage Controller", "SCSI Bus Controller", "IDE Controller", "Floppy Disk Controller", "IPI Bus Controller",
                   "RAID Controller", "ATA Controller", "Serial ATA Controller", "Serial Attached SCSI Controller",
                   "Non-Volatile Memory Controller">(),
        parse_info<"Network Controller", "Ethernet Controller", "Token Ring Controller", "FDDI Controller", "ATM Controller",
                   "ISDN Controller", "WorldFip Controller", "PICMG 2.14 Multi Computing Controller", "Infiniband Controller",
                   "Fabric Controller">(),
        parse_info<"Display Controller", "VGA Compatible Controller", "XGA Controller", "3D Controller (Not VGA-Compatible)">(),
        parse_info<"Multimedia Controller", "Multimedia Video Controller", "Multimedia Audio Controller", "Computer Telephony Device",
                   "Audio Device">(),
        parse_info<"Memory Controller", "RAM Controller", "Flash Controller">(),
        parse_info<"Bridge", "Host Bridge", "ISA Bridge", "EISA Bridge", "MCA Bridge", "PCI-to-PCI Bridge", "PCMCIA Bridge", "NuBus Bridge",
                   "CardBus Bridge", "RACEway Bridge", "PCI-to-PCI Bridge">(),
        parse_info<"Simple Communication Controller", "Serial Controller", "Parallel Controller", "Multiport Serial Controller", "Modem",
                   "IEEE 488.1/2 (GPIB) Controller", "Smart Card Controller">(),
        parse_info<"Base System Peripheral", "PIC", "DMA Controller", "Timer", "RTC Controller", "PCI Hot-Plug Controller",
                   "SD Host controller", "IOMMU">(),
        parse_info<"Input Device Controller", "Keyboard Controller", "Digitizer Pen", "Mouse Controller", "Scanner Controller",
                   "Gameport Controller">(),
        parse_info<"Docking Station", "Generic">(),
        parse_info<"Processor", "386", "486", "Pentium", "Pentium Pro">(),
        parse_info<"Serial Bus Controller", "FireWire (IEEE 1394) Controller", "ACCESS Bus Controller", "SSA", "USB Controller",
                   "Fibre Channel", "SMBus Controller", "InfiniBand Controller", "IPMI Interface", "SERCOS Interface (IEC 61491)",
                   "CANbus Controller">(),
        parse_info<"Wireless Controller", "iRDA Compatible Controller", "Consumer IR Controller">(),
        parse_info<"Intelligent Controller", "I20">(),
        parse_info<"Satellite Communication Controller", "Unknown", "Satellite TV Controller", "Satellite Audio Controller",
                   "Satellite Voice Controller", "Satellite Data Controller">(),
        parse_info<"Encryption Controller", "Network and Computing Encrpytion/Decryption">(),
        parse_info<"Signal Processing Controller", "DPIO Modules", "Performance Counters">(),
        parse_info<"Processing Accelerator">(),
        parse_info<"Non-Essential Instrumentation">(),
    };

    void reset(vfs::devfs::Sequence* seq) {
        seq->index = reinterpret_cast<uint64_t>(devices.head);
        seq->eof = false;
    }

    void next(vfs::devfs::Sequence* seq) {
        const auto node = reinterpret_cast<stl::LinkedList<Device>::Node*>(seq->index);

        seq->index = reinterpret_cast<uint64_t>(node->next);
        seq->eof = node->next == nullptr;
    }

    void show(vfs::devfs::Sequence* seq) {
        const auto& device = reinterpret_cast<stl::LinkedList<Device>::Node*>(seq->index)->item;

        const ClassInfo* info = nullptr;
        if (device.class_code < sizeof(CLASSES) / sizeof(ClassInfo)) info = &CLASSES[device.class_code];

        const auto class_name = info != nullptr ? info->name : "Unknown";
        const auto subclass_name = info != nullptr ? info->get_subclass_name(device.subclass) : "Unknown";

        seq->printf("%02d:%02d:%02d\n", device.bus_num, device.num, device.function_num);
        seq->printf("   class: 0x%X (%s)\n", device.class_code, class_name);
        seq->printf("   subclass: 0x%X (%s)\n", device.subclass, subclass_name);
        seq->printf("   vendor_id: 0x%X\n", device.vendor_id);
        seq->printf("   device_id: 0x%X\n", device.device_id);
    }

    static vfs::devfs::SequenceOps ops = {
        .reset = reset,
        .next = next,
        .show = show,
    };

    void init(vfs::Node* node) {
        for (auto bus = 0u; bus < 256; bus++) {
            for (auto device = 0u; device < 32; device++) {
                check_device(bus, device);
            }
        }

        vfs::devfs::register_sequence_device(node, "pci", &ops);
    }
} // namespace cosmos::devices::pci
