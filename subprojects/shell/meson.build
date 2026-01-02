project(
    'shell',
    ['cpp']
)

stl = subproject('stl')

executable(
    'shell',
    [
        'utils.cpp',
        'nanoprintf.cpp',
        'font.cpp',
        'terminal.cpp',
        'commands.cpp',
        'main.cpp'
    ],
    include_directories : [
        '../../vendor/nanoprintf',
        stl.get_variable('includes')
    ],
    cpp_args : [
        '-fno-omit-frame-pointer',
        '-O0', '-g'
    ],
    pie : false
)
