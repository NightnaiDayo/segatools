@echo off

set SEGATOOLS_CONFIG_PATH=.\segatools_satellite.ini

pushd %~dp0

rem start "AM Daemon" /min inject -d -k soulhook.dll tools\amdaemon\amdaemon -c tools\amdaemon\config_common.json tools\amdaemon\config_satellite.json tools\amdaemon\config_terminal.json tools\amdaemon\config_emoney_satellite_pro.json tools\amdaemon\config_emoney_terminal_pro.json
start "AM Daemon" /min inject -d -k soulhook.dll tools\amdaemon\amdaemon -c tools\amdaemon\config_common.json tools\amdaemon\config_satellite.json tools\amdaemon\config_terminal.json

timeout 5 > nul

inject -d -k soulhook.dll fbb.exe -img -full -fhd -nodbg

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause