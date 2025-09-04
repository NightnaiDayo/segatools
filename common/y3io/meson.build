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
        'impl/dummy/y3io.c',
        'impl/websockets/y3io.c',
        'impl/websockets/3rdparty/websockets/aw-base64.h',
        'impl/websockets/3rdparty/websockets/aw-sha1.h',
        'impl/websockets/3rdparty/websockets/websocket.c',
        'impl/websockets/3rdparty/websockets/websocket.h',
        'impl/websockets/3rdparty/cjson/cJSON.c',
        'impl/websockets/3rdparty/cjson/cJSON.h',
        'impl/websockets/3rdparty/cjson/cJSON_Utils.c',
        'impl/websockets/3rdparty/cjson/cJSON_Utils.h',
        'impl/y3io.h',
    ],
)
