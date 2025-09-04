y3io_lib = static_library(
    'y3io',
    name_prefix : '',
    include_directories: inc,
    implicit_include_directories : false,
    dependencies : [
        capnhook.get_variable('hook_dep'),
        capnhook.get_variable('hooklib_dep'),
    ],
    link_with : [
        util_lib,
    ],
    sources : [
        'impl/dummy/y3io.c',
        'impl/y3io.h',
    ],
)
