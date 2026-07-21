@echo off
set SEGATOOLS_CONFIG_PATH=.\segatools_satellite.ini

pushd %~dp0

start "AM Daemon" /min inject -d -k hkbhook.dll amdaemon.exe -c common.json satellite.json terminal.json config_hook_satellite.json

inject -d -k hkbhook.dll hkb.exe -logFile satellite.log -screen-fullscreen 0 -popupwindow -screen-quality Fantastic

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause