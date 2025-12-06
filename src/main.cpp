#include "devices/atapio.hpp"
#include "devices/framebuffer.hpp"
#include "devices/keyboard.hpp"
#include "devices/pit.hpp"
#include "devices/ps2kbd.hpp"
#include "gdt.hpp"
#include "interrupts/isr.hpp"
#include "limine.hpp"
#include "log/devfs.hpp"
#include "log/log.hpp"
#include "memory/heap.hpp"
#include "memory/offsets.hpp"
#include "memory/physical.hpp"
#include "memory/virtual.hpp"
#include "scheduler/scheduler.hpp"
#include "serial.hpp"
#include "shell/shell.hpp"
#include "utils.hpp"
#include "vfs/devfs.hpp"
#include "vfs/ramfs.hpp"
#include "vfs/vfs.hpp"

using namespace cosmos;

void init() {
    devices::pit::start();
    if (!devices::ps2kbd::init()) utils::halt();

    vfs::register_filesystem("ramfs", vfs::ramfs::init);
    vfs::register_filesystem("devfs", vfs::devfs::init);

    const auto ramfs = vfs::mount("/");
    vfs::ramfs::init(ramfs, "");

    const auto devfs = vfs::mount("/dev");
    vfs::devfs::init(devfs, "");

    log::init_devfs(devfs);
    devices::framebuffer::init(devfs);
    devices::keyboard::init(devfs);
    devices::atapio::init(devfs);

    INFO("Initialized");

    log::disable_display();
    scheduler::create_process(shell::run);
}

extern "C" [[noreturn]]
void main() {
    asm volatile("cli" ::: "memory");
    serial::init();
    limine::init();

    log::enable_display();
    INFO("Starting");

    gdt::init();
    isr::init();

    memory::phys::init();

    uint64_t rsp;
    asm volatile("mov %%rsp, %0" : "=r"(rsp));
    rsp = memory::virt::DIRECT_MAP + memory::virt::get_phys(rsp);
    asm volatile("mov %0, %%rsp" ::"ri"(rsp));

    const auto space = memory::virt::create();
    if (space == 0) utils::panic(nullptr, "Failed to create virtual address space");
    memory::virt::switch_to(space);
    log::enable_paging();

    memory::heap::init();

    scheduler::create_process(init, space);
    scheduler::run();

    utils::halt();
}
