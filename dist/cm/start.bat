@echo off

pushd %~dp0

start "AM Daemon" /min inject -d -k cmhook.dll amdaemon.exe -c config_common.json config_server.json config_client.json config_hook.json
inject -d -k cmhook.dll CardMaker.exe -screen-fullscreen 0 -popupwindow -screen-width 1080 -screen-height 1920

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause