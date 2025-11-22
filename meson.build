project(
    'cosmos-os',
    [ 'cpp' ],
    default_options: [
        'cpp_std=c++23'
    ]
)

executable(
    'cosmos-os',
    [
        'src/utils.cpp',
        'src/limine.cpp',
        'src/nanoprintf.cpp',
        'src/serial.cpp',
        'src/interrupts/pic.cpp',
        'src/interrupts/isr.cpp',
        'src/memory/physical.cpp',
        'src/memory/virtual.cpp',
        'src/memory/heap.cpp',
        'src/scheduler/scheduler.cpp',
        'src/devices/pit.cpp',
        'src/devices/ps2kbd.cpp',
        'src/shell/font.cpp',
        'src/shell/commands.cpp',
        'src/shell/shell.cpp',
        'src/main.cpp'
    ],
    include_directories: include_directories(
        'src',
        'vendor/limine',
        'vendor/nanoprintf'
    ),
    cpp_args: [
        '-std=c++23',
        '-ffreestanding',
        '-m64', '-march=x86-64',
        '-mno-red-zone', '-fno-omit-frame-pointer',
        '-O0', '-g',
        '-mno-mmx', '-mno-3dnow', '-mno-sse', '-mno-avx', '-mno-avx2', '-mno-avx512f',
        '-fno-stack-protector', '-fno-stack-check',
        '-fno-exceptions', '-fno-rtti',
        '-fno-asynchronous-unwind-tables',
        '-pedantic', '-Wall', '-Wextra', '-Wundef', '-Werror', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-function', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers'
    ],
    pie: false,
    link_args: [
        '-ffreestanding', '-nostdlib',
        '-pie',
        '-T' + meson.project_source_root() + '/linker.ld'
    ]
)
