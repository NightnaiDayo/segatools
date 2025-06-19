@echo off
pushd %~dp0

set DAEMON_WAIT_SECONDS=5

set AMDAEMON_CFG=config_common.json ^
config_ch.json ^
config_ex.json ^
config_jp.json ^
config_st1_ch.json ^
config_st1_ex.json ^
config_st1_jp.json ^
config_st2_ch.json ^
config_st2_ex.json ^
config_st2_jp.json ^
config_st3_ch.json ^
config_st3_ex.json ^
config_st3_jp.json ^
config_st4_ch.json ^
config_st4_ex.json ^
config_st4_jp.json ^
config_laninstall_server_ch.json ^
config_laninstall_client1_ch.json ^
config_laninstall_client2_ch.json ^
config_laninstall_client3_ch.json ^
config_laninstall_server_ex.json ^
config_laninstall_client1_ex.json ^
config_laninstall_client2_ex.json ^
config_laninstall_client3_ex.json ^
config_laninstall_server_jp.json ^
config_laninstall_client1_jp.json ^
config_laninstall_client2_jp.json ^
config_laninstall_client3_jp.json ^
config_hook.json

start /min "AM Daemon" inject -d -k tokyohook.dll amdaemon.exe -c %AMDAEMON_CFG%
timeout %DAEMON_WAIT_SECONDS% > nul 2>&1

REM ---------------------------------------------------------------------------
REM Set configuration
REM ---------------------------------------------------------------------------

REM Configuration values to be passed to the game executable.
REM All known values:
REM -forceapi:11
REM -forcehal
REM -forcevsync:0/1
REM -fullscreen
REM -windowed
REM Note: -windowed is recommended as the game looks sharper in windowed mode.
inject -d -k tokyohook.dll app.exe -windowed

taskkill /f /im amdaemon.exe > nul 2>&1

echo.
echo Game processes have terminated
pause
