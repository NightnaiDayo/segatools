@echo off

pushd %~dp0

REM Root directory
set ROOT_DIR=WindowsNoEditor

rem Matching Server paths
set MATCHING_SERVER_FILE_NAME=tdrserver.exe
set MATCHING_SERVER_PATH=..\Tools\%MATCHING_SERVER_FILE_NAME%

rem AM Daemon paths
set DAEMON_DIR=%ROOT_DIR%\AMDaemon
set DAEMON_CONFIG_PATH=%DAEMON_DIR%\config.json
rem Make sure to use appdata=appdata in segatools.ini
set DAEMON_CHECK_LAN_SERVER_PATH=appdata\SDDS\LanServer.dat

rem Check if LanServer.dat is present
if exist "%DAEMON_CHECK_LAN_SERVER_PATH%" (
  set DAEMON_LAN_CONFIG_PATH=%DAEMON_DIR%\config_LanServer.json

  start "matching_server" /min %MATCHING_SERVER_PATH%
) else (
  set DAEMON_LAN_CONFIG_PATH=%DAEMON_DIR%\config_LanClient.json
)

start "AM Daemon" /min inject -d -k swdchook.dll "%DAEMON_DIR%\amdaemon.exe" -c %DAEMON_CONFIG_PATH% -c %DAEMON_LAN_CONFIG_PATH% config_hook.json

REM Launch Todoroki
set APP_EXE_DIR=WindowsNoEditor\Todoroki\Binaries\Win64
set APP_EXE_PATH=%APP_EXE_DIR%\Todoroki-Win64-Shipping.exe

REM Valid -launch parameters are "Cabinet" or "MiniCabinet"
inject -d -k swdchook.dll "%APP_EXE_PATH%" -launch="MiniCabinet" -ABSLOG="..\Userdata\Todoroki.log" -UserDir="..\Userdata" -NotInstalled -UNATTENDED

taskkill /f /im amdaemon.exe > nul 2>&1
taskkill /f /im tdrserver.exe > nul 2>&1

echo.
echo Game processes have terminated
pause