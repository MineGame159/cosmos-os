#include "interrupts/isr.hpp"
#include "limine.hpp"
#include "memory/heap.hpp"
#include "memory/offsets.hpp"
#include "memory/physical.hpp"
#include "memory/virtual.hpp"
#include "scheduler/scheduler.hpp"
#include "serial.hpp"
#include "utils.hpp"

using namespace cosmos;

void process1() {
    memory::virt::map_pages(memory::virt::get_current(), 0, memory::phys::alloc_pages(1) / 4096ul, 1, false);

    const auto i = reinterpret_cast<uint32_t*>(4);
    *i = 0;

    for (; *i < 5; (*i)++) {
        serial::printf("[process 1] Hi %d\n", *i);
        scheduler::yield();
    }
}

void process2() {
    memory::virt::map_pages(memory::virt::get_current(), 0, memory::phys::alloc_pages(1) / 4096ul, 1, false);

    const auto i = reinterpret_cast<uint32_t*>(4);
    *i = 0;

    for (; *i < 10; (*i)++) {
        serial::printf("[process 2] Hello %d\n", *i);

        if (*i == 4) scheduler::exit();
        scheduler::yield();
    }
}

void init() {
    serial::printf("[cosmos] %s\n", "Initialized");

    serial::printf("[cosmos] Total memory: %d mB\n", static_cast<uint64_t>(memory::phys::get_total_pages()) * 4096 / 1024 / 1024);
    serial::printf("[cosmos] Free memory: %d mB\n", static_cast<uint64_t>(memory::phys::get_free_pages()) * 4096 / 1024 / 1024);

    const auto pixels = reinterpret_cast<uint32_t*>(memory::virt::FRAMEBUFFER);
    pixels[0] = 0xFFFFFFFF;
    pixels[1] = 0xFFFF0000;
    pixels[2] = 0xFF00FF00;
    pixels[3] = 0xFF0000FF;

    scheduler::create_process(process1);
    scheduler::create_process(process2);
}

extern "C" [[noreturn]]
void main() {
    serial::init();

    if (!limine::init()) {
        utils::halt();
    }

    isr::init();
    memory::phys::init();

    const auto space = memory::virt::create();
    memory::virt::switch_to(space);

    memory::heap::init();

    scheduler::init();
    scheduler::create_process(init, space);
    scheduler::run();

    utils::halt();
}
