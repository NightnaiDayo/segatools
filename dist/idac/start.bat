@echo off

pushd %~dp0

REM set the APP_DIR to the Y drive
set APP_DIR=Y:\SDGT

REM create the APP_DIR if it doesn't exist and redirect it to the TEMP folder
if not exist "%APP_DIR%" (
    subst Y: %TEMP%
    REM timeout /t 1
    if not exist "%APP_DIR%" (
        mkdir "%APP_DIR%"
    )
)

echo Mounted the Y:\ drive to the %TEMP%\SDGT folder

set AMDAEMON_CFG=config_common.json ^
config_ex.json ^
config_jp.json ^
config_laninstall_client_ex.json ^
config_laninstall_client_jp.json ^
config_laninstall_server_ex.json ^
config_laninstall_server_jp.json ^
config_aime_high_ex.json ^
config_aime_high_jp.json ^
config_aime_normal_ex.json ^
config_aime_normal_jp.json ^
config_seat_1_ex.json ^
config_seat_1_jp.json ^
config_seat_2_ex.json ^
config_seat_2_jp.json ^
config_seat_3_ex.json ^
config_seat_3_jp.json ^
config_seat_4_ex.json ^
config_seat_4_jp.json ^
config_seat_single_ex.json ^
config_seat_single_jp.json ^
config_hook.json

start "AM Daemon" /min inject -d -k idachook.dll amdaemon.exe -c %AMDAEMON_CFG%
inject -d -k idachook.dll ..\WindowsNoEditor\GameProject.exe -culture=en launch=Cabinet ABSLOG="..\..\..\..\..\Userdata\GameProject.log" -Master -UserDir="..\..\..\Userdata" -NotInstalled -UNATTENDED
taskkill /f /im amdaemon.exe > nul 2>&1

REM unmount the APP_DIR
subst Y: /d > nul 2>&1

echo.
echo Game processes have terminated
pause