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

    {
        constexpr auto str = "Hello, World!";

        const auto file = vfs::open("/hello.txt", vfs::Mode::Write);
        file->ops->write(file->handle, str, utils::strlen(str));
        vfs::close(file);

        serial::printf("[init] Written '%s'\n", str);
    }

    {
        const auto file = vfs::open("/hello.txt", vfs::Mode::Read);

        const auto length = file->ops->seek(file->handle, vfs::SeekType::End, 0);
        file->ops->seek(file->handle, vfs::SeekType::Start, 0);

        const auto str = static_cast<char*>(memory::heap::alloc(length + 1));
        file->ops->read(file->handle, str, length);
        str[length] = '\0';

        vfs::close(file);

        serial::printf("[init] Read '%s'\n", str);
        memory::heap::free(str);
    }

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

    scheduler::init();
    scheduler::create_process(init, space);
    scheduler::run();

    utils::halt();
}
