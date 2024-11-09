@echo off
set BUILD_DIR=build
set BUILD_DIR_32=%BUILD_DIR%\build32
set BUILD_DIR_64=%BUILD_DIR%\build64
set BUILD_DIR_ZIP=%BUILD_DIR%\zip
set DIST_DIR=dist
set DOC_DIR=doc

REM Set Your Visual Studio install path if this one was wrong
set VS_INSTALLATION=C:\Program Files\Microsoft Visual Studio\2022\Community

if /I "%1"=="build" (
    call :build
    exit
)
if /I "%1"=="zip" (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "& './package.ps1'"
    exit
)

echo %~nx0 [action]
echo     build: Build the for both x86 and x64
echo     zip: Make zip file
exit

:build (
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

    :build_x64 (
        call %VSVARALL% x64

        if exist %BUILD_DIR_64% (
            %MESON% setup %BUILD_DIR_64% --backend vs --buildtype release --reconfigure
        ) else (
            %MESON% setup %BUILD_DIR_64% --backend vs --buildtype release
        )

        pushd %BUILD_DIR_64%
        msbuild /m /p:Configuration=release /p:Platform=x64 segatools.sln
        popd
    )

    :build_x86 (
        call %VSVARALL% x86

        if exist %BUILD_DIR_32% (
            %MESON% setup %BUILD_DIR_32% --backend vs --buildtype release --reconfigure
        ) else (
            %MESON% setup %BUILD_DIR_32% --backend vs --buildtype release
        )

        pushd %BUILD_DIR_32%
        msbuild /m /p:Configuration=release /p:Platform=Win32 segatools.sln
        popd
    )

    :end (
        endlocal
    )
)
