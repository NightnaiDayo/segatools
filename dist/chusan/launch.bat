@echo off

pushd %~dp0

start "AM Daemon" /min inject_x64 -d -k chusanhook_x64.dll amdaemon.exe -c config_common.json config_server.json config_client.json config_cvt.json config_sp.json config_hook.json
inject_x86 -d -k chusanhook_x86.dll chusanApp.exe

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause