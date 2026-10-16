@echo off
rem Builds dist\pmsyncgd.exe with MinGW-w64 (x86_64). Needs gcc and windres on PATH.
setlocal
cd /d "%~dp0"

if not exist build mkdir build
if not exist dist mkdir dist

windres res\pmsyncgd.rc -O coff -o build\pmsyncgd.res.o || goto :fail

gcc -std=gnu11 -O2 -Wall -Wextra -municode -D__USE_MINGW_ANSI_STDIO=1 ^
    src\main.c src\ui.c src\patcher.c src\gdfind.c src\live.c build\pmsyncgd.res.o ^
    -static -s -ladvapi32 -o dist\pmsyncgd.exe || goto :fail

echo built dist\pmsyncgd.exe
exit /b 0

:fail
echo build failed
exit /b 1
