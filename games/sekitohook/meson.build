shared_library(
    'sekitohook',
    name_prefix : '',
    include_directories : inc,
    implicit_include_directories : false,
    vs_module_defs : 'sekitohook.def',
    dependencies : [
        capnhook.get_variable('hook_dep'),
        capnhook.get_variable('hooklib_dep')
    ],
    link_with : [
        aimeio_lib,
        amex_lib,
        board_lib,
        hooklib_lib,
        gfxhook_lib,
        jvs_lib,
        platform_lib,
        sekitoio_lib,
        util_lib,
        y3io_lib,
    ],
    sources : [
        'config.c',
        'config.h',
        'dllmain.c',
        'jvs.c',
        'jvs.h',
        'sekito-dll.c',
        'sekito-dll.h',
    ],
)
