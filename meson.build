project(
    'cosmos-os',
    ['cpp', 'c'],
    default_options : [
        'cpp_std=c++23'
    ]
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
    'src/scheduler/event.cpp',
    'src/scheduler/scheduler.cpp',
    'src/acpi/uacpi.cpp',
    'src/acpi/acpi.cpp',
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
    '-ffreestanding',
    '-m64', '-march=x86-64',
    '-mno-red-zone', '-fno-omit-frame-pointer',
    '-O0', '-g',
    '-mno-mmx', '-mno-3dnow', '-mno-sse', '-mno-avx', '-mno-avx2', '-mno-avx512f',
    '-fno-stack-protector', '-fno-stack-check',
    '-fno-asynchronous-unwind-tables',
    '-pedantic', '-Wall', '-Wextra', '-Wundef', '-Werror', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-function', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers', '-Wno-gnu-zero-variadic-macro-arguments'
]

cpp_args = [
    '-std=c++23',
    '-fno-exceptions', '-fno-rtti',
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
    cpp_args : c_args + cpp_args,
    pie : false,
    link_args : [
        '-ffreestanding', '-nostdlib',
        '-pie',
        '-T' + meson.project_source_root() + '/linker.ld'
    ]
)

subproject('shell')
