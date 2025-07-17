@echo off

cd /d %~dp0

set PATH=%~dp0lib;%~dp0;X:\;%PATH%

:BEGIN
cd /d %~dp0

qprocess amdaemon.exe > NUL
IF %ERRORLEVEL% NEQ 0 start /min cmd /C "inject -d -k apm3hook.dll amdaemon.exe -f -c daemon_config\common.json daemon_config\server.json config_hook.json"

inject -d -k apm3hook.dll APMV3System -screen-fullscreen 0 -screen-width 1920 -screen-height 1080 -popupWindow -logFile output_log.txt

if exist %tmp%\segaboot (
  del %tmp%\segaboot
  goto END
)

if exist %tmp%\app (
  del %tmp%\app
  goto APP
)

if exist %tmp%\apptest (
  del %tmp%\apptest
  goto APPTEST
)

goto END

:APP

call W:\game.bat
taskkill /f /im emoneyUI.exe > nul 2>&1
goto BEGIN

:APPTEST
call W:\gametest.bat
goto BEGIN

:END

taskkill /f /im emoneyUI.exe > nul 2>&1
taskkill /f /im amdaemon.exe > nul 2>&1

rundll32 apm3hook.dll,UnmountApmDrives

echo.
echo Game processes have terminated
pause