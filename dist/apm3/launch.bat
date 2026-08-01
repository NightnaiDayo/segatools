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

rem detect model and dipswitch for server/client

set AMDAEMON_CONFIG=standalone.json
inject -w -k apm3hook.dll hwvalue.exe modeltype
if %ERRORLEVEL% == 5 goto BEGIN

inject -w -k apm3hook.dll hwvalue.exe dipsw
set AMDAEMON_CONFIG=client.json
if %ERRORLEVEL% == 1 set AMDAEMON_CONFIG=server.json

:BEGIN

pushd %~dp0

set DOORSTOP_DISABLE=TRUE
qprocess amdaemon.exe > NUL
IF %ERRORLEVEL% NEQ 0 start /min "AM Daemon" inject -d -k apm3hook.dll amdaemon.exe -c daemon_config\common.json daemon_config\%AMDAEMON_CONFIG% config_hook.json
set DOORSTOP_DISABLE=

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

pushd C:\
call W:\game.bat
popd

taskkill /f /im emoneyUI.exe > nul 2>&1
goto BEGIN

:APPTEST
pushd C:\
call W:\gametest.bat
popd

goto BEGIN

:END

taskkill /f /im emoneyUI.exe > nul 2>&1
taskkill /f /im amdaemon.exe > nul 2>&1

rundll32 apm3hook.dll,UnmountApmDrives

echo.
echo Game processes have terminated
pause
