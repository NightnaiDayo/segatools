@echo off
REM Set Your Visual Studio install path if this one was wrong
set VS_INSTALLATION=C:\Program Files\Microsoft Visual Studio\2022\Community

REM Check VC++
set VSVARALL="%VS_INSTALLATION%\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VSVARALL% (
    echo Build tools for Microsoft Visual C++ 2022 is not Installed
    echo Edit this file to change the VS_INSTALLATION if you are installed the Visual Studio in other path
    goto end
)

REM Check Meson
for /f "tokens=* usebackq" %%i in (`where meson`) do set MESON="%%i"
if not exist %MESON% (
    echo Meson is not Installed
    goto end
)

:build_x64
call %VSVARALL% x64

if exist build\build_x64 (
    %MESON% setup build/build_x64 --backend vs --buildtype release --reconfigure
) else (
    %MESON% setup build/build_x64 --backend vs --buildtype release
)

pushd build\build_x64
msbuild /m /p:Configuration=release /p:Platform=x64 segatools.sln
popd

:build_x86
call %VSVARALL% x86

if exist build\build_x86 (
    %MESON% setup build/build_x86 --backend vs --buildtype release --reconfigure
) else (
    %MESON% setup build/build_x86 --backend vs --buildtype release
)

pushd build\build_x86
msbuild /m /p:Configuration=release /p:Platform=Win32 segatools.sln
popd

:end
exit
