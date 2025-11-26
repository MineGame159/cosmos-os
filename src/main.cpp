#include "devices/pit.hpp"
#include "devices/ps2kbd.hpp"
#include "interrupts/isr.hpp"
#include "limine.hpp"
#include "memory/heap.hpp"
#include "memory/physical.hpp"
#include "memory/virtual.hpp"
#include "scheduler/scheduler.hpp"
#include "serial.hpp"
#include "shell/shell.hpp"
#include "utils.hpp"
#include "vfs/ramfs.hpp"

using namespace cosmos;

void init() {
    devices::pit::start();
    devices::ps2kbd::init();
    vfs::ramfs::create(vfs::mount("/"));

    serial::printf("[cosmos] %s\n", "Initialized");

    scheduler::create_process(shell::run);
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

    scheduler::create_process(init, space);
    scheduler::run();

    utils::halt();
}
