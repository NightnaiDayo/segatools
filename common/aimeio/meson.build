aimeio_lib = static_library(
    'aimeio',
    name_prefix : '',
    include_directories: inc,
    implicit_include_directories : false,
    dependencies : [
        segapi.get_variable('api_dep'),
    ],
    link_with : [
        util_lib,
    ],
    sources : [
        'aimeio.c',
    ],
)
