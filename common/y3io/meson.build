y3io_lib = static_library(
    'y3io',
    name_prefix : '',
    include_directories: inc,
    implicit_include_directories : false,
    dependencies : [
        capnhook.get_variable('hook_dep'),
        capnhook.get_variable('hooklib_dep'),
        cwinwebsocket.get_variable('cws_dep'),
    ],
    link_with : [
        util_lib,
    ],
    sources : [
        'impl/websockets/config.c',
        'impl/websockets/config.h',
        'impl/websockets/y3ws.c',
        'impl/websockets/y3ws.h',
        'impl/websockets/3rdparty/cjson/cJSON.c',
        'impl/websockets/3rdparty/cjson/cJSON.h',
        'impl/websockets/3rdparty/cjson/cJSON_Utils.c',
        'impl/websockets/3rdparty/cjson/cJSON_Utils.h',
        'impl/y3io.h',
    ],
)
