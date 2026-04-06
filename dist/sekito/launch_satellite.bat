@echo off
set SEGATOOLS_CONFIG_PATH=.\segatools_satellite.ini

pushd %~dp0

start "AM Daemon" /min inject_x64 -d -k sekitohook_x64.dll bin\amdaemon.exe -c bin\config_new.json  -c bin\config_video_single.json -c bin\config_video_multi.json -c bin\config_input_sate.json -c bin\config_input_terminal.json -c bin\config_input_terminal_exp.json -c config_hook_satellite.json

inject_x86 -d -k sekitohook_x86.dll bin\appSate.exe

taskkill /f /im appSate.exe > nul 2>&1
taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause