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

where ssh >nul 2>nul
if errorlevel 1 (
    echo OpenSSH client was not found on PATH.
    echo Install the Windows OpenSSH client or add ssh.exe to PATH.
    exit /b 1
)

set "REMOTE=%PI_USER%@%PI_HOST%"

echo Stopping previous Raspberry Pi game process if needed...
ssh "%REMOTE%" "pkill -f '[s]teering_wheel_console' || true"
if errorlevel 1 (
    echo Failed to contact %REMOTE% or stop the previous game process.
    exit /b 1
)

echo Launching Raspberry Pi game...
ssh -n "%REMOTE%" "cd '%PI_REMOTE_DIR%' && (nohup setsid ./build/bin/steering_wheel_console >/tmp/steering_wheel_console.log 2>&1 </dev/null &)"
if errorlevel 1 (
    echo Failed to launch the Raspberry Pi game.
    exit /b 1
)

echo Raspberry Pi game launched.
echo Remote log: /tmp/steering_wheel_console.log
