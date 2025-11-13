#include "interrupts/isr.hpp"
#include "limine.hpp"
#include "serial.hpp"
#include "utils.hpp"

using namespace cosmos;

extern "C" [[noreturn]] void main() {
    serial::init();

    if (!limine::init()) {
        utils::halt();
    }

    isr::init();

    serial::printf("[cosmos] %s\n", "Initialized");

    const auto pixels = static_cast<uint32_t*>(limine::get_framebuffer().pixels);
    pixels[0] = 0xFFFFFFFF;
    pixels[1] = 0xFFFF0000;
    pixels[2] = 0xFF00FF00;
    pixels[3] = 0xFF0000FF;

    utils::halt();
}
