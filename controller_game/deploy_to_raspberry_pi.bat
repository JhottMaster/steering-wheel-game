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
tar -cf "%ARCHIVE%" README.md Makefile.raspberry_pi src/main.cpp src/game_logic.h src/game_logic_tests.cpp src/platform_linux.h assets tools
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
ssh "%REMOTE%" "cd '%PI_REMOTE_DIR%' && tar -xf controller_game_sync.tar && rm -f controller_game_sync.tar && find . -exec touch {} +"
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

echo Stopping previous Pi game process if needed...
ssh "%REMOTE%" "pkill -f build/bin/steering_wheel_console || true"

echo Launching new Pi build...
ssh -n "%REMOTE%" "cd '%PI_REMOTE_DIR%' && (nohup setsid ./build/bin/steering_wheel_console >/tmp/steering_wheel_console.log 2>&1 </dev/null &)"
if errorlevel 1 (
    echo Build succeeded, but launching the game failed.
    del /f /q "%ARCHIVE%" >nul 2>nul
    exit /b 1
)

del /f /q "%ARCHIVE%" >nul 2>nul

echo.
echo Raspberry Pi deploy completed successfully.
echo Remote log: /tmp/steering_wheel_console.log
