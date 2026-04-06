@echo off

pushd %~dp0

start "AM Daemon" /min inject -d -k mai2hook.dll amdaemon.exe -f -c config_common.json config_server.json config_client.json config_hook.json
inject -d -k mai2hook.dll sinmai -screen-fullscreen 0 -popupwindow -screen-width 2160 -screen-height 1920  -silent-crashes

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause