#pragma once

#include "stl/bit_field.hpp"
#include "stl/span.hpp"

#include <cstdint>

namespace cosmos::elf {
    enum class ProgramHeaderType : uint32_t {
        Null = 0,
        Load = 1,
        Dynamic = 2,
        Interpret = 3,
        Note = 4,
        Shlib = 5,
        ProgramHeader = 6,
        ThreadLocalStorage = 7,
        LowOs = 0x60000000,
        HighOs = 0x6FFFFFFF,
        LowProcessor = 0x70000000,
        HighProcessor = 0x7FFFFFFF,
    };

    enum class ProgramHeaderFlags : uint32_t {
        Execute = 0x01,
        Write = 0x02,
        Read = 0x04,
    };
    ENUM_BIT_FIELD(ProgramHeaderFlags)

    struct ProgramHeader {
        ProgramHeaderType type;
        ProgramHeaderFlags flags;
        uint64_t file_offset;
        uint64_t virt_offset;
        uint64_t phys_offset;
        uint64_t file_size;
        uint64_t virt_size;
        uint64_t alignment;
    };

    enum class SectionHeaderType : uint32_t {
        Null = 0x0,
        Data = 0x1,
        Symbols = 0x2,
        Strings = 0x3,
        RelocationsAddends = 0x4,
        Hashes = 0x5,
        Dynamic = 0x6,
        Notes = 0x7,
        Bss = 0x8,
        Relocations = 0x9,
        DynamicSymbols = 0x0B,
        Constructors = 0x0E,
        Destructors = 0x0F,
        PreConstructors = 0x10,
        Group = 0x11,
        ExtendedSectionIndices = 0x12,
        Count = 0x13,
        LowOs = 0x60000000,
        HighOs = 0x6FFFFFFF,
        LowProcessor = 0x70000000,
        HighProcessor = 0x7FFFFFFF,
    };

    enum class SectionHeaderFlags : uint64_t {
        Write = 0x01,
        Alloc = 0x02,
        Execute = 0x04,
        Merge = 0x10,
        Strings = 0x20,
        InfoLink = 0x40,
        LinkOrder = 0x80,
        NonConformingOs = 0x100,
        Group = 0x200,
        Tls = 0x400,
        Os = 0x0FF00000,
        Processor = 0xF0000000,
        Ordered = 0x4000000,
        Exclude = 0x8000000,
    };
    ENUM_BIT_FIELD(SectionHeaderFlags)

    struct SectionHeader {
        uint32_t name_offset;
        SectionHeaderType type;
        SectionHeaderFlags flags;
        uint64_t virt_offset;
        uint64_t file_offset;
        uint64_t size;
        uint32_t link;
        uint32_t info;
        uint64_t alignment;
        uint64_t entry_size;
    };

    enum class BinaryType : uint16_t {
        Unknown = 0x01,
        Relocatable = 0x01,
        Executable = 0x02,
        Shared = 0x03,
        Core = 0x04,
        LowOs = 0xFE00,
        HighOs = 0xFEFF,
        LowProcessor = 0xFF00,
        HighProcessor = 0xFFFF,
    };

    struct Binary {
        BinaryType type;
        uint64_t virt_entry;
        stl::Span<ProgramHeader> program_headers;
        stl::Span<SectionHeader> section_headers;
    };
} // namespace cosmos::elf
