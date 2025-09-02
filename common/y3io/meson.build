y3io_lib = static_library(
    'y3io',
    name_prefix : '',
    include_directories: inc,
    implicit_include_directories : false,
    dependencies : [
        capnhook.get_variable('hook_dep'),
        capnhook.get_variable('hooklib_dep')
    ],
    link_with : [
        util_lib
    ],
    sources : [
        'config.c',
        'config.h',
        'y3-dll.c',
        'y3-dll.h',
        'y3.c',
        'y3.h',
        'impl/dummy/y3io.c',
        'impl/dummy/y3io.h',
    ],
)
