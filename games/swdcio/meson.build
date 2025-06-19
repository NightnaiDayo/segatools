swdcio_lib = static_library(
    'swdccio',
    name_prefix : '',
    include_directories : inc,
    implicit_include_directories : false,
    dependencies : [
        dinput8_lib,
        dxguid_lib,
        xinput_lib,
    ],
    link_with : [
        util_lib,
    ],
    sources : [
        'backend.h',
        'config.c',
        'config.h',
        'di.c',
        'di.h',
        'di-dev.c',
        'di-dev.h',
        'dllmain.c',
        'swdcio.h',
        'wnd.c',
        'wnd.h',
        'xi.c',
        'xi.h',
    ],
)
