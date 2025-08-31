@echo off
set SEGATOOLS_CONFIG_PATH=..\segatools_satellite.ini

pushd %~dp0

pushd PackageBase
start /min "AM Daemon" ..\inject -d -k ..\ekthook.dll amdaemon.exe -c config_sate.json ..\config_hook.json
popd
pushd exe
..\inject -d -k ..\ekthook.dll ekt.exe -logfile game.log -screen-fullscreen 0 -screen-width 1920 -screen-height 1080 -screen-quality Ultra -silent-crashes
popd
exit /b 0

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause