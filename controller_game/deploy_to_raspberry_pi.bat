@echo off
setlocal EnableExtensions

cd /d "%~dp0"

if not exist "raspberry_pi_config.bat" (
    echo Missing raspberry_pi_config.bat.
    echo Copy raspberry_pi_config.example.bat to raspberry_pi_config.bat and fill in your Pi settings.
    exit /b 1
)

call "raspberry_pi_config.bat"

if "%PI_HOST%"=="" (
    echo PI_HOST is not set.
    exit /b 1
)

if "%PI_USER%"=="" (
    echo PI_USER is not set.
    exit /b 1
)

if "%PI_REMOTE_DIR%"=="" (
    echo PI_REMOTE_DIR is not set.
    exit /b 1
)

if "%PI_RAYLIB_DIR%"=="" (
    echo PI_RAYLIB_DIR is not set.
    exit /b 1
)

where ssh >nul 2>nul
if errorlevel 1 (
    echo OpenSSH client was not found on PATH.
    echo Install the Windows OpenSSH client or add ssh.exe to PATH.
    exit /b 1
)

where scp >nul 2>nul
if errorlevel 1 (
    echo scp was not found on PATH.
    echo Install the Windows OpenSSH client or add scp.exe to PATH.
    exit /b 1
)

where tar >nul 2>nul
if errorlevel 1 (
    echo tar was not found on PATH.
    echo Windows tar.exe is required for packaging the upload.
    exit /b 1
)

set "REMOTE=%PI_USER%@%PI_HOST%"
set "ARCHIVE=%TEMP%\controller_game_pi_sync.tar"

if exist "%ARCHIVE%" del /f /q "%ARCHIVE%"

echo Packaging controller_game for Raspberry Pi...
tar -cf "%ARCHIVE%" README.md DEVELOPMENT_GUIDELINES.md Makefile.raspberry_pi src/main.cpp src/app src/game src/input src/views src/platform/platform_linux.h assets tools
if errorlevel 1 (
    echo Failed to create upload archive.
    exit /b 1
)

echo Ensuring remote folder exists...
ssh "%REMOTE%" "mkdir -p '%PI_REMOTE_DIR%'"
if errorlevel 1 (
    echo Failed to create remote directory %PI_REMOTE_DIR%.
    del /f /q "%ARCHIVE%" >nul 2>nul
    exit /b 1
)

echo Uploading files to %REMOTE%:%PI_REMOTE_DIR% ...
scp "%ARCHIVE%" "%REMOTE%:%PI_REMOTE_DIR%/controller_game_sync.tar"
if errorlevel 1 (
    echo Upload failed.
    del /f /q "%ARCHIVE%" >nul 2>nul
    exit /b 1
)

echo Extracting files on Raspberry Pi...
ssh "%REMOTE%" "cd '%PI_REMOTE_DIR%' && tar -xf controller_game_sync.tar && rm -f controller_game_sync.tar && find README.md DEVELOPMENT_GUIDELINES.md Makefile.raspberry_pi src assets tools -type f -exec touch {} +"
if errorlevel 1 (
    echo Remote extract failed.
    del /f /q "%ARCHIVE%" >nul 2>nul
    exit /b 1
)

echo Building on Raspberry Pi...
ssh "%REMOTE%" "cd '%PI_REMOTE_DIR%' && make -f Makefile.raspberry_pi RAYLIB_DIR='%PI_RAYLIB_DIR%'"
if errorlevel 1 (
    echo.
    echo Remote build failed.
    del /f /q "%ARCHIVE%" >nul 2>nul
    exit /b 1
)

del /f /q "%ARCHIVE%" >nul 2>nul

echo.
echo Raspberry Pi deploy/build completed successfully.
echo To run it on the Pi display, use:
echo   run_raspberry_pi_game.bat
