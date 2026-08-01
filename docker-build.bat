@echo off
setlocal enabledelayedexpansion

:: Static Environment Variables
set IMAGE_NAME=djhackers/segatools-build:latest
set CONTAINER_NAME=segatools-build

:: Main Execution
docker build . -t %IMAGE_NAME%

if ERRORLEVEL 1 (
    goto failure
)

docker run -it --rm -v "%~dp0:/segatools" --name %CONTAINER_NAME% %IMAGE_NAME%

if ERRORLEVEL 1 (
    goto failure
)

:: docker image rm -f %IMAGE_NAME%

goto success

:failure
echo segatools Docker build FAILED!
goto finish

:success
echo segatools Docker build completed successfully.
goto finish

:finish
pause
