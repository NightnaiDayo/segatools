@echo off
set SEGATOOLS_CONFIG_PATH=.\segatools_satellite.ini

set SEGATOOLS_VFS_RELATIVE_PATH=..\..\exe
set SEGATOOLS_CONFIG_PATH=%SEGATOOLS_VFS_RELATIVE_PATH%\%SEGATOOLS_CONFIG_PATH%

pushd ..\PackageBase\am_capture
start "AM Capture" /min %SEGATOOLS_VFS_RELATIVE_PATH%\inject_x86.exe -d -k %SEGATOOLS_VFS_RELATIVE_PATH%\ekthook_x86.dll AmCapture.exe
popd

set SEGATOOLS_CONFIG_PATH=.\segatools_satellite.ini
set SEGATOOLS_VFS_RELATIVE_PATH=

pushd %~dp0
start "AM Daemon" /min inject_x64.exe -d -k ekthook_x64.dll ..\PackageBase\amdaemon.exe -c ..\PackageBase\config_sate.json config_hook.json

inject_x64 -d -k ekthook_x64.dll ekt.exe -logfile satellite.log -screen-fullscreen 1 -screen-width 1920 -screen-height 1080 -screen-quality Ultra -silent-crashes

taskkill /f /im AmCapture.exe > nul 2>&1
taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause