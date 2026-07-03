@echo off
setlocal

set "MSYS2_ROOT=C:\msys64"
set "MINGW_BIN=%MSYS2_ROOT%\mingw64\bin"
set "USR_BIN=%MSYS2_ROOT%\usr\bin"

cd /d "%~dp0"

if not exist "%MINGW_BIN%\g++.exe" (
    echo MinGW g++ was not found at "%MINGW_BIN%\g++.exe".
    echo Install the MSYS2 MINGW64 toolchain first.
    exit /b 1
)

if not exist "%MINGW_BIN%\mingw32-make.exe" (
    echo mingw32-make was not found at "%MINGW_BIN%\mingw32-make.exe".
    echo Install the MSYS2 MINGW64 toolchain first.
    exit /b 1
)

set "PATH=%MINGW_BIN%;%USR_BIN%;%PATH%"

echo Running controller game logic tests...
"%MINGW_BIN%\mingw32-make.exe" -f Makefile.windows test

if errorlevel 1 (
    echo.
    echo Tests failed.
    exit /b 1
)

echo.
echo Tests completed successfully.
