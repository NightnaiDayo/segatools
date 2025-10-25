@echo off
set SEGATOOLS_CONFIG_PATH=.\segatools_terminal.ini

pushd %~dp0

start "AM Daemon" /min inject_x64 -d -k ekthook_x64.dll ..\PackageBase\amdaemon.exe -c ..\PackageBase\config_terminal.json config_hook.json
inject_x64 -d -k ekthook_x64.dll ekt.exe -terminal -logfile terminal.log -screen-fullscreen 1 -screen-width 1920 -screen-height 1080 -screen-quality Ultra -silent-crashes

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause