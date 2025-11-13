#include "limine.hpp"
#include "serial.hpp"
#include <limine.h>

__attribute__((unused, section(".requests_start"))) //
static volatile uint64_t start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((unused, section(".requests"))) //
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(4);

__attribute__((unused, section(".requests"))) //
static volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
};

__attribute__((unused, section(".requests_end"))) //
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

namespace cosmos::limine {
    static Framebuffer fb;

    void init_framebuffer() {
        const auto limine_fb = framebuffer_request.response->framebuffers[0];

        fb = {
            .width = static_cast<uint32_t>(limine_fb->width),
            .height = static_cast<uint32_t>(limine_fb->height),
            .pitch = static_cast<uint32_t>(limine_fb->pitch / 4),
            .pixels = limine_fb->address,
        };
    }

    bool init() {
        if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision)) {
            serial::print("[limine] Base revision not supported\n");
            return false;
        }

        if (framebuffer_request.response == nullptr || framebuffer_request.response->framebuffer_count < 1) {
            serial::print("[limine] Framebuffer missing\n");
            return false;
        }

        init_framebuffer();

        serial::print("[limine] Initialized\n");
        return true;
    }

    const Framebuffer& get_framebuffer() { return fb; }
} // namespace cosmos::limine
