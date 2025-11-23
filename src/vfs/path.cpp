#include "path.hpp"

#include "memory/heap.hpp"
#include "utils.hpp"

namespace cosmos::vfs {
    uint32_t check_abs_path(const char* path) {
        if (*path != '/') return 0;
        auto length = 1u;

        while (path[length] != '\0') {
            if (path[length] == '/' && path[length - 1] == '/') {
                return 0;
            }

            if (path[length] == ' ' && (path[length - 1] == '/' || path[length + 1] == '/' || path[length + 1] == '\0')) {
                return 0;
            }

            length++;
        }

        if (length > 1 && path[length - 1] == '/') {
            length--;
        }

        return length;
    }

    bool PathEntryIt::next() {
        const auto prev_entry = entry;
        entry += length;

        while (*entry == '/') {
            entry++;
        }

        if (*entry == '\0') {
            entry = prev_entry;
            return false;
        }

        length = 0;
        while (entry[length] != '/' && entry[length] != '\0') {
            length++;
        }

        return true;
    }

    PathEntryIt iterate_path_entries(const char* path) {
        return {
            .entry = path,
            .length = 0,
        };
    }

    // Helper to append a segment to a vector-like char buffer
    static void append_segment(char* out, uint32_t& out_len, const char* seg, const uint32_t seg_len) {
        if (out_len > 1) {
            // not root, add separator
            out[out_len++] = '/';
        }

        for (uint32_t i = 0; i < seg_len; i++) {
            out[out_len++] = seg[i];
        }
        out[out_len] = '\0';
    }

    char* resolve_path(const char* cwd, const char* path) {
        if (path == nullptr) return nullptr;

        const char* trimmed = utils::str_trim_left(path);
        if (*trimmed == '\0') {
            const auto len = check_abs_path(cwd);
            if (len == 0) return nullptr;

            const auto out = static_cast<char*>(memory::heap::alloc(len + 1));
            utils::memcpy(out, cwd, len);
            out[len] = '\0';
            return out;
        }

        // Absolute path: validate and return copy
        if (trimmed[0] == '/') {
            const auto len = check_abs_path(trimmed);
            if (len == 0) return nullptr;

            const auto out = static_cast<char*>(memory::heap::alloc(len + 1));
            utils::memcpy(out, trimmed, len);
            out[len] = '\0';
            return out;
        }

        // Relative path: join cwd + '/' + path then normalize
        const auto cwd_len = check_abs_path(cwd);
        if (cwd_len == 0) return nullptr;

        uint32_t path_len = 0;
        while (trimmed[path_len] != '\0') path_len++;

        const auto joined_len = cwd_len + 1 + path_len + 1;
        const auto joined = static_cast<char*>(memory::heap::alloc(joined_len));

        utils::memcpy(joined, cwd, cwd_len);
        joined[cwd_len] = '/';
        utils::memcpy(&joined[cwd_len + 1], trimmed, path_len);
        joined[cwd_len + 1 + path_len] = '\0';

        const auto segments = static_cast<char*>(memory::heap::alloc(joined_len));
        uint32_t seg_len = 0; // current length of the normalized path buffer

        if (auto it = iterate_path_entries(joined); it.next()) {
            do {
                const char* entry = it.entry;
                const auto entry_length = it.length;

                // Handle '.' -> skip
                if (entry_length == 1 && entry[0] == '.') continue;

                // Handle '..' -> pop last segment if possible
                if (entry_length == 2 && entry[0] == '.' && entry[1] == '.') {
                    // If we're at root (seg_len == 0), cannot go above root
                    if (seg_len == 0) {
                        memory::heap::free(joined);
                        memory::heap::free(segments);
                        return nullptr;
                    }

                    // Remove last segment: find last '/' in segments
                    uint32_t i = seg_len;
                    while (i > 0 && segments[i - 1] != '/') i--;
                    seg_len = i > 0 ? i - 1 : 0;
                    segments[seg_len] = '\0';
                    continue;
                }

                // Normal segment -> append
                append_segment(segments, seg_len, entry, entry_length);
            } while (it.next());
        }

        if (seg_len == 0) {
            const auto out = static_cast<char*>(memory::heap::alloc(2));
            out[0] = '/';
            out[1] = '\0';
            memory::heap::free(joined);
            memory::heap::free(segments);
            return out;
        }

        // segments currently holds the path without leading '/'
        const auto out_len = seg_len + 1;
        const auto out = static_cast<char*>(memory::heap::alloc(out_len + 1));
        out[0] = '/';
        utils::memcpy(&out[1], segments, seg_len);
        out[out_len] = '\0';

        memory::heap::free(joined);
        memory::heap::free(segments);

        return out;
    }
} // namespace cosmos::vfs
