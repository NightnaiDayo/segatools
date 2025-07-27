@echo off

pushd %~dp0

start "AM Daemon" /min inject -d -k idzhook.dll amdaemon.exe -c configDHCP_Final_Common.json configDHCP_Final_JP.json configDHCP_Final_JP_ST1.json configDHCP_Final_JP_ST2.json configDHCP_Final_EX.json configDHCP_Final_EX_ST1.json configDHCP_Final_EX_ST2.json

rem Set dipsw1=0 and uncomment the ServerBox for in store battle?
rem inject -k idzhook.dll ServerBoxD8_Nu_x64.exe
inject -d -k idzhook.dll InitialD0_DX11_Nu.exe -m

taskkill /f /im ServerBoxD8_Nu_x64.exe > nul 2>&1
taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause