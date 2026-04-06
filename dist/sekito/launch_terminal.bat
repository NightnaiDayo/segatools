@echo off
set SEGATOOLS_CONFIG_PATH=.\segatools_terminal.ini

pushd %~dp0

start "AM Daemon" /min inject_x64 -d -k sekitohook_x64.dll bin\amdaemon.exe -c bin\config_new.json  -c bin\config_video_single.json -c bin\config_video_multi.json -c bin\config_input_sate.json -c bin\config_input_terminal.json -c bin\config_input_terminal_exp.json -c config_hook_terminal.json

start "SKT Server Processes" /min cmd /C bin\server\server_start.bat

inject_x86 -d -k sekitohook_x86.dll bin\appTerminal.exe

taskkill /f /im appTerminal.exe > nul 2>&1
taskkill /f /im amdaemon.exe > nul 2>&1

call bin\server\server_stop.bat

echo.
echo Game processes have terminated
pause