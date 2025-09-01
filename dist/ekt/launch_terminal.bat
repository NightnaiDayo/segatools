@echo off
set SEGATOOLS_CONFIG_PATH=..\segatools_terminal.ini

pushd %~dp0

start /min "AM Daemon" inject -d -k ekthook.dll PackageBase\amdaemon.exe -c PackageBase\config_terminal.json config_hook.json
inject -d -k ekthook.dll exe\ekt.exe -logfile game.log -screen-fullscreen 0 -screen-width 1920 -screen-height 1080 -screen-quality Ultra -silent-crashes

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause