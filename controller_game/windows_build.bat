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

%SystemRoot%\System32\tasklist.exe /FI "IMAGENAME eq steering_wheel_game.exe" 2>NUL | %SystemRoot%\System32\find.exe /I "steering_wheel_game.exe" >NUL
if not errorlevel 1 (
    echo Stopping existing steering wheel game process...
    %SystemRoot%\System32\taskkill.exe /F /IM steering_wheel_game.exe >NUL
    if errorlevel 1 (
        echo Failed to stop the existing game process.
        exit /b 1
    )
)

echo Building steering wheel controller game...
"%MINGW_BIN%\mingw32-make.exe" -f Makefile.windows buildonly

if errorlevel 1 (
    echo.
    echo Build failed.
    echo Makefile.windows first checks for a local raylib checkout at:
    echo   ..\raylib
    echo If that is missing, it falls back to the MSYS2 MINGW64 raylib package.
    echo You can also set RAYLIB_DIR before running this script.
    exit /b 1
)

echo.
echo Build completed successfully.
echo Built executable: "%CD%\build\bin\steering_wheel_game.exe"
