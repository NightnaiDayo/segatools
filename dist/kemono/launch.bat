@echo off

pushd %~dp0

set PATH=%cd%\amdaemon.exe.local;%PATH%
start "AM Daemon" /min inject_x64 -d -k kemonohook_x64.dll amdaemon.exe -c config.json
set PATH=%cd%\UnityApp\Parade.exe.local;%PATH%
inject_x86 -d -k kemonohook_x86.dll UnityApp\Parade -screen-fullscreen 0 -popupwindow -screen-width 720 -screen-height 1280 -silent-crashes

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause
