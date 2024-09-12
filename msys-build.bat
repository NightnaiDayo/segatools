@echo off
chcp 65001
C:\msys64\usr\bin\bash --login -c "DIR=%cd:\=\\\\%; cd $(cygpath $DIR) && PATH=/mingw32/bin/:$PATH make build32 && PATH=/mingw64/bin/:$PATH make build64 && make clean-zip && PATH=/mingw64/bin/:$PATH make zip"
pause