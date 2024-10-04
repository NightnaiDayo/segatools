shared_library(
    'swdchook',
    name_prefix : '',
    include_directories : inc,
    implicit_include_directories : false,
    vs_module_defs : 'swdchook.def',
    c_pch : '../precompiled.h',
    dependencies : [
        capnhook.get_variable('hook_dep'),
        capnhook.get_variable('hooklib_dep'),
        xinput_lib,
    ],
    link_with : [
        aimeio_lib,
        board_lib,
        hooklib_lib,
        swdcio_lib,
        platform_lib,
        util_lib,
    ],
    sources : [
        'config.c',
        'config.h',
        'dllmain.c',
        'swdc-dll.c',
        'swdc-dll.h',
        'io4.c',
        'io4.h',
        'zinput.c',
        'zinput.h',
        'ffb.c',
        'ffb.h'
    ],
)
