shared_library(
    'ekthook',
    name_prefix : '',
    include_directories : inc,
    implicit_include_directories : false,
    vs_module_defs : 'ekthook.def',
    dependencies : [
        capnhook.get_variable('hook_dep'),
        capnhook.get_variable('hooklib_dep')
    ],
    link_with : [
        aimeio_lib,
        board_lib,
        ektio_lib,
        hooklib_lib,
        jvs_lib,
        platform_lib,
        unityhook_lib,
        util_lib,
        y3io_lib,
    ],
    sources : [
        'config.c',
        'config.h',
        'dllmain.c',
        'ekt-dll.c',
        'ekt-dll.h',
        'io4.c',
        'io4.h',
    ],
)
