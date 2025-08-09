@echo off

pushd %~dp0

set PATH=%~dp0lib;%~dp0;X:\;%PATH%

rem remove the reboot flag and show the copyright screen

if exist %tmp%\APMv3SystemReboot (
  del %tmp%\APMv3SystemReboot
)

if exist %tmp%\SequenceSetting.json (
  del %tmp%\SequenceSetting.json
)

:BEGIN
pushd %~dp0

qprocess amdaemon.exe > NUL
IF %ERRORLEVEL% NEQ 0 start /min "AM Daemon" inject -d -k apm3hook.dll amdaemon.exe -c daemon_config\common.json daemon_config\server.json config_hook.json

REM Add "-screen-fullscreen 0 -popupWindow" if you want to run in windowed mode
inject -d -k apm3hook.dll APMV3System -logFile output_log.txt

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
