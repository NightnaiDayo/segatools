@echo off
set SEGATOOLS_CONFIG_PATH=.\segatools_satellite.ini

pushd %~dp0

start "AM Daemon" /min inject_x64 -d -k sekitohook_x64.dll amdaemon.exe -c config_new.json  -c config_video_single.json -c config_video_multi.json -c config_input_sate.json -c config_input_terminal.json -c config_input_terminal_exp.json -c config_hook.json

call server\server_start.bat

inject_x86 -d -k sekitohook_x86.dll appTerminal.exe

taskkill /f /im appTerminal.exe > nul 2>&1
taskkill /f /im amdaemon.exe > nul 2>&1

call server\server_stop.bat

echo.
echo Game processes have terminated
pause