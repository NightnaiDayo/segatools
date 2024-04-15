project(
    'segatools',
    'c',
    version: '0.1.0',
    default_options: [
        'werror=true',
    ],
)

add_project_arguments(
    '-DCOBJMACROS',
    '-DDIRECTINPUT_VERSION=0x0800',
    '-DWIN32_LEAN_AND_MEAN',
    '-D_WIN32_WINNT=_WIN32_WINNT_WIN7',
    '-DMINGW_HAS_SECURE_API=1',
    '-Wno-unused',
    language: 'c',
)

cc = meson.get_compiler('c')

if cc.get_id() != 'msvc'
    add_project_arguments(
        '-ffunction-sections',
        '-fdata-sections',
        '-flto',  # Enable Link-Time Optimization
        language: 'c',
    )

    add_project_link_arguments(
        '-Wl,--enable-stdcall-fixup',
        '-Wl,--exclude-all-symbols',
        '-Wl,--gc-sections',
        '-static-libgcc',
        '-flto',  # Enable Link-Time Optimization
        '-Wl,-s',  # Strip debug symbols
        language: 'c',
    )
endif

shlwapi_lib = cc.find_library('shlwapi')
dinput8_lib = cc.find_library('dinput8')
dxguid_lib = cc.find_library('dxguid')
xinput_lib = cc.find_library('xinput')
pathcch_lib = cc.find_library('pathcch')

inc = include_directories('.')
capnhook = subproject('capnhook')

subdir('amex')
subdir('iccard')
subdir('board')
subdir('hooklib')
subdir('jvs')
subdir('platform')
subdir('util')

subdir('gfxhook')
subdir('unityhook')

subdir('aimeio')
subdir('chuniio')
subdir('divaio')
subdir('carolio')
subdir('idzio')
subdir('idacio')
subdir('swdcio')
subdir('mu3io')
subdir('mai2io')
subdir('cmio')
subdir('mercuryio')
subdir('cxbio')
subdir('fgoio')

subdir('chunihook')
subdir('divahook')
subdir('carolhook')
subdir('idzhook')
subdir('idachook')
subdir('swdchook')
subdir('minihook')
subdir('chusanhook')
subdir('mu3hook')
subdir('mai2hook')
subdir('cmhook')
subdir('mercuryhook')
subdir('cxbhook')
subdir('fgohook')
