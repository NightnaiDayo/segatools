chuniio_lib = static_library(
    'chuniio',
    name_prefix : '',
    include_directories : inc,
    implicit_include_directories : false,
    c_pch : '../precompiled.h',
    link_with : [
        util_lib
    ],

    sources : [
        'chu2to3.c',
        'chu2to3.h',
        'chuniio.c',
        'chuniio.h',
        'config.c',
        'config.h',
        'leddata.h',
        'ledoutput.c',
        'ledoutput.h',
        'pipeimpl.c',
        'pipeimpl.h',
        'serialimpl.c',
        'serialimpl.h'
    ],
)
