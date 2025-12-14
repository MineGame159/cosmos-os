#include "parser.hpp"

#include "log/log.hpp"

namespace cosmos::elf {
    enum class Class : uint8_t {
        Bit32 = 1,
        Bit64 = 2,
    };

    enum class Data : uint8_t {
        Lsb = 1,
        Msb = 2,
    };

    struct [[gnu::packed]] Identifier {
        char magic[4];
        Class class_;
        Data data;
        uint8_t version;
        uint8_t abi;
        uint8_t abi_version;
        uint8_t _padding[7];
    };

    struct [[gnu::packed]] Header {
        BinaryType type;
        uint16_t machine;
        uint32_t version;
        uint64_t entry;
        uint64_t file_program_headers_offset;
        uint64_t file_section_headers_offset;
        uint32_t flags;
        uint16_t size;
        uint16_t program_header_size;
        uint16_t program_header_count;
        uint16_t section_header_size;
        uint16_t section_header_count;
        uint16_t section_names_index;
    };

    static_assert(sizeof(Identifier) == 16, "Invalid Identifier struct");
    static_assert(sizeof(Header) == 48, "Invalid Header struct");
    static_assert(sizeof(ProgramHeader) == 56, "Invalid ProgramHeader struct");
    static_assert(sizeof(SectionHeader) == 64, "Invalid SectionHeader struct");

    constexpr uint64_t IDENTIFIER_HEADER_SIZE = sizeof(Identifier) + sizeof(Header);

    Binary* parse(vfs::File* file) {
        // Read identifier and header
        uint8_t identifier_header[IDENTIFIER_HEADER_SIZE];

        if (file->ops->read(file, &identifier_header[0], IDENTIFIER_HEADER_SIZE) != IDENTIFIER_HEADER_SIZE) {
            ERROR("Failed to read identifier + header");
            return nullptr;
        }

        const auto identifier = reinterpret_cast<Identifier*>(&identifier_header[0]);
        const auto header = reinterpret_cast<Header*>(identifier + 1);

        // Validate identifier
        if (identifier->magic[0] != 0x7F || identifier->magic[1] != 'E' || identifier->magic[2] != 'L' || identifier->magic[3] != 'F') {
            ERROR("Invalid identifier magic");
            return nullptr;
        }

        if (identifier->class_ != Class::Bit64) {
            ERROR("Invalid identifier class, only supports 64 bit binaries");
            return nullptr;
        }

        if (identifier->data != Data::Lsb) {
            ERROR("Invalid identifier data, only supports little endian binaries");
            return nullptr;
        }

        if (identifier->version != 1) {
            ERROR("Invalid identifier version, only supports 1");
            return nullptr;
        }

        if (identifier->abi != 0x00 && identifier->abi != 0x03) {
            ERROR("Invalid identifier abi, only supports Linux");
            return nullptr;
        }

        // Validate header
        if (header->machine != 0x3E) {
            ERROR("Invalid header machine, only supports x86-64");
            return nullptr;
        }

        if (header->version != 1) {
            ERROR("Invalid header version, only supports 1");
            return nullptr;
        }

        // Create binary
        const auto programs_size = sizeof(ProgramHeader) * header->program_header_count;
        const auto sections_size = sizeof(SectionHeader) * header->section_header_count;

        const auto binary = static_cast<Binary*>(memory::heap::alloc(sizeof(Binary) + programs_size + sections_size, alignof(Binary)));

        binary->type = header->type;
        binary->virt_entry = header->entry;

        const auto programs_ptr = reinterpret_cast<ProgramHeader*>(binary + 1);
        binary->program_headers = { programs_ptr, header->program_header_count };

        const auto sections_ptr = reinterpret_cast<SectionHeader*>(reinterpret_cast<uint8_t*>(binary) + sizeof(Binary) + programs_size);
        binary->section_headers = { sections_ptr, header->section_header_count };

        // Read program headers
        file->ops->seek(file, vfs::SeekType::Start, static_cast<int64_t>(header->file_program_headers_offset));

        if (file->ops->read(file, programs_ptr, programs_size) != programs_size) {
            ERROR("Failed to read program headers");
            memory::heap::free(binary);
            return nullptr;
        }

        // Read section headers
        file->ops->seek(file, vfs::SeekType::Start, static_cast<int64_t>(header->file_section_headers_offset));

        if (file->ops->read(file, sections_ptr, sections_size) != sections_size) {
            ERROR("Failed to read section headers");
            memory::heap::free(binary);
            return nullptr;
        }

        return binary;
    }
} // namespace cosmos::elf
