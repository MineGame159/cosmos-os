#include "acpi/acpi.hpp"
#include "devices/atapio.hpp"
#include "devices/framebuffer.hpp"
#include "devices/info.hpp"
#include "devices/keyboard.hpp"
#include "devices/pci.hpp"
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
#include "memory/virt_range_alloc.hpp"
#include "memory/virtual.hpp"
#include "scheduler/scheduler.hpp"
#include "serial.hpp"
#include "syscalls/init.hpp"
#include "tss.hpp"
#include "utils.hpp"
#include "vfs/devfs.hpp"
#include "vfs/iso9660.hpp"
#include "vfs/ramfs.hpp"
#include "vfs/vfs.hpp"

using namespace cosmos;

void init() {
    acpi::init();

    if (!devices::ps2kbd::init()) utils::halt();

    vfs::ramfs::register_filesystem();
    vfs::devfs::register_filesystem();
    vfs::iso9660::register_filesystem();

    vfs::mount("/", "ramfs", "");
    const auto devfs = vfs::mount("/dev", "devfs", "");

    log::init_devfs(devfs);
    devices::pit::init(devfs);
    devices::framebuffer::init(devfs);
    devices::keyboard::init(devfs);
    devices::atapio::init(devfs);
    devices::info::init(devfs);
    devices::pci::init(devfs);

    vfs::mount("/iso", "iso9660", "/dev/ata01");

    INFO("Initialized");

    log::disable_display();
    scheduler::create_process("/iso/shell", "/");

    scheduler::exit(0);
}

extern "C" [[noreturn]]
void main() {
    asm volatile("cli" ::: "memory");
    serial::init();
    limine::init();

    log::enable_display();
    INFO("Starting");

    gdt::init();
    tss::init();
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
    memory::virt::init_range_alloc();
    syscalls::init();

    scheduler::StackFrame frame;
    scheduler::setup_dummy_frame(frame, init);
    scheduler::create_process(init, space, scheduler::Land::Kernel, frame, "/");

    scheduler::run();

    utils::halt();
}
