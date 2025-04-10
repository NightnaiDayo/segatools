@echo off

cd /d %~dp0

inject -d -k fgohook.dll ago.exe

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause