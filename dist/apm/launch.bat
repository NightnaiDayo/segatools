@echo off

cd /d %~dp0

set PATH=%~dp0lib;%~dp0;X:\;%PATH%

FOR /F "tokens=* USEBACKQ" %%F IN (`powershell -ExecutionPolicy Bypass -Command "& {(Get-Content -Path segatools.ini | Where-Object { $_ -match 'option=' }).Split('=')[1]}"`) DO (
	SET optionDir=%%F
)
ECHO Game Data directory: %optionDir%

:BEGIN
cd /d %~dp0

qprocess amdaemon.exe > NUL
IF %ERRORLEVEL% NEQ 0 start /min cmd /C "inject -d -k apmhook.dll amdaemon.exe -f -c daemon_config\common.json daemon_config\server.json config_hook.json"

inject -d -k apmhook.dll APMV3System -screen-fullscreen 0 -screen-width 1920 -screen-height 1080

if exist %optionDir%\segaboot (
  del %optionDir%\segaboot
  goto END
)

if exist %optionDir%\app (
  del %optionDir%\app
  goto APP
)

if exist %optionDir%\apptest (
  del %optionDir%\apptest
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

echo.
echo Game processes have terminated
pause