project(
    'shell',
    ['cpp'],
    default_options : [
        'cpp_std=c++23'
    ]
)

executable(
    'shell',
    [
        'main.cpp'
    ],
    cpp_args : [
        '-std=c++23',
        '-ffreestanding',
        '-m64', '-march=x86-64',
        '-fno-omit-frame-pointer',
        '-O0', '-g',
        '-mno-mmx', '-mno-3dnow', '-mno-sse', '-mno-avx', '-mno-avx2', '-mno-avx512f',
        '-fno-stack-protector', '-fno-stack-check',
        '-fno-exceptions', '-fno-rtti',
        '-fno-asynchronous-unwind-tables',
        '-pedantic', '-Wall', '-Wextra', '-Wundef', '-Werror', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-function', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers'
    ],
    pie : false,
    link_args : [
        '-ffreestanding', '-nostdlib'
    ]
)
