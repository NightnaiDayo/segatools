@echo off

pushd %~dp0

start "AM Daemon" /min inject -d -k soulhook.dll .\tools\amdaemon\amdaemon.exe -c .\tools\amdaemon\config_common.json .\tools\amdaemon\config_emoney_satellite_pro.json .\tools\amdaemon\config_satellite.json

inject -d -k soulhook.dll fbb.exe

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause