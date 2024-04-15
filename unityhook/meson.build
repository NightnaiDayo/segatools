unityhook_lib = static_library(
    'unityhook',
    include_directories: inc,
    implicit_include_directories: false,
    c_pch: '../precompiled.h',
    dependencies: [
        capnhook.get_variable('hook_dep'),
        pathcch_lib
    ],
    sources: [
        'mono.h',
        'config.c',
        'config.h',
        'doorstop.c',
        'doorstop.h',
        'hook.c',
        'hook.h',
        'util.h'
    ],
)
