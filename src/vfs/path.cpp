#include "path.hpp"

#include "memory/heap.hpp"
#include "stl/span.hpp"
#include "utils.hpp"

namespace cosmos::vfs {
    uint32_t check_abs_path(const stl::StringView path) {
        if (path[0] != '/') return 0;
        auto length = 1u;

        for (; length < path.size(); length++) {
            if (path[length] == '/' && path[length - 1] == '/') {
                return 0;
            }

            if (path[length] == ' ' && (path[length - 1] == '/' || path[length + 1] == '/' || path[length + 1] == '\0')) {
                return 0;
            }
        }

        if (length > 1 && path[length - 1] == '/') {
            length--;
        }

        return length;
    }

    static stl::StringView alloc_copy(const stl::StringView str) {
        const auto copy = memory::heap::alloc_array<char>(str.size() + 1);

        utils::memcpy(copy, str.data(), str.size());
        copy[str.size()] = '\0';

        return stl::StringView(copy, str.size());
    }

    template <std::same_as<stl::StringView>... T>
    static stl::StringView alloc_append(const T... parts) {
        const uint64_t size = (0 + ... + parts.size());

        const auto copy = memory::heap::alloc_array<char>(size + 1);
        auto current = copy;

        // ReSharper disable once CppDFAUnusedValue
        ((utils::memcpy(current, parts.data(), parts.size()), current += parts.size()), ...);
        copy[size] = '\0';

        return stl::StringView(copy, size);
    }

    stl::StringView join(const stl::StringView a, const stl::StringView b) {
        if (a.empty() && b.empty()) return alloc_copy("");
        if (a.empty()) return alloc_copy(b);
        if (b.empty()) return alloc_copy(a);

        const auto slash_count = (a[a.size() - 1] == '/' ? 1 : 0) + (b[0] == '/' ? 1 : 0);

        if (slash_count == 2) return alloc_append(a, b.substr(1));
        if (slash_count == 1) return alloc_append(a, b);

        return alloc_append(a, stl::StringView("/"), b);
    }

    static bool push_segment(stl::StringView* segments, uint32_t& segment_count, stl::StringView segment) {
        segment = segment.trim();
        if (segment.empty()) return true;

        if (segment == ".") return true;

        if (segment == "..") {
            if (segment_count == 0) return false;
            segment_count--;
        } else {
            if (segment_count == 32) return false;
            segments[segment_count++] = segment;
        }

        return true;
    }

    static stl::StringView join_abs(const stl::Span<stl::StringView> segments) {
        // Return / if there are no segments
        if (segments.empty()) return alloc_copy("/");

        // Join segments
        uint64_t size = 0;

        for (const auto& segment : segments) {
            size += 1 + segment.size();
        }

        const auto path = memory::heap::alloc_array<char>(size + 1);
        auto current = path;

        for (const auto& segment : segments) {
            *current = '/';
            utils::memcpy(current + 1, segment.data(), segment.size());
            current += 1 + segment.size();
        }

        *current = '\0';
        return stl::StringView(path, size);
    }

    stl::StringView resolve(stl::StringView cwd, const stl::StringView path) {
        // Path is already absolute
        if (path.size() >= 1 && path[0] == '/') {
            const auto size = check_abs_path(path);
            return alloc_copy(path.substr(0, size));
        }

        // Check that cwd is absolute
        {
            const auto size = check_abs_path(cwd);
            if (size == 0) return alloc_copy("");

            cwd = cwd.substr(0, size);
        }

        // Split both cwd and path into segments and store them on stack
        stl::StringView segments[32];
        uint32_t segment_count = 0;

        {
            auto it = stl::split(cwd, '/');

            while (it.next()) {
                if (!push_segment(segments, segment_count, it.entry)) {
                    return alloc_copy("");
                }
            }
        }

        {
            auto it = stl::split(path, '/');

            while (it.next()) {
                if (!push_segment(segments, segment_count, it.entry)) {
                    return alloc_copy("");
                }
            }
        }

        return join_abs(stl::Span(segments, segment_count));
    }
} // namespace cosmos::vfs
