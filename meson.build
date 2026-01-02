project(
    'cosmos-os',
    ['cpp', 'c']
)

sources = [
    'src/utils.cpp',
    'src/limine.cpp',
    'src/nanoprintf.cpp',
    'src/serial.cpp',
    'src/gdt.cpp',
    'src/tss.cpp',
    'src/log/font.cpp',
    'src/log/display.cpp',
    'src/log/log.cpp',
    'src/log/devfs.cpp',
    'src/interrupts/pic.cpp',
    'src/interrupts/isr.cpp',
    'src/memory/physical.cpp',
    'src/memory/virtual.cpp',
    'src/memory/virt_range_alloc.cpp',
    'src/memory/heap.cpp',
    'src/elf/parser.cpp',
    'src/elf/loader.cpp',
    'src/syscalls/init.cpp',
    'src/syscalls/handlers.cpp',
    'src/task/process.cpp',
    'src/task/event.cpp',
    'src/task/pipe.cpp',
    'src/task/scheduler.cpp',
    'src/acpi/uacpi.cpp',
    'src/acpi/acpi.cpp',
    'src/devices/null.cpp',
    'src/devices/pit.cpp',
    'src/devices/framebuffer.cpp',
    'src/devices/ps2kbd.cpp',
    'src/devices/keyboard.cpp',
    'src/devices/atapio.cpp',
    'src/devices/info.cpp',
    'src/devices/pci.cpp',
    'src/vfs/path.cpp',
    'src/vfs/vfs.cpp',
    'src/vfs/ramfs.cpp',
    'src/vfs/devfs.cpp',
    'src/vfs/iso9660.cpp',
    'src/main.cpp'
]

includes = [
    'src',
    'vendor/limine',
    'vendor/nanoprintf'
]

c_args = [
    '-mno-red-zone', '-fno-omit-frame-pointer',
    '-O0', '-g'
]

stl_proj = subproject('stl')
stl_dep = stl_proj.get_variable('stl_dep')

uacpi = subproject('uacpi')
sources += uacpi.get_variable('sources')
includes += uacpi.get_variable('includes')

c_args += [
    '-DUACPI_BAREBONES_MODE',
    '-DUACPI_FORMATTED_LOGGING'
]

executable(
    'cosmos-os',
    sources,
    include_directories : includes,
    dependencies : [stl_dep],
    c_args : c_args,
    cpp_args : c_args,
    pie : false,
    link_args : [
        '-pie',
        '-T' + meson.project_source_root() + '/linker.ld'
    ]
)

subproject('shell')
