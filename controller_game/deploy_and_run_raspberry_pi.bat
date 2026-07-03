@echo off
setlocal EnableExtensions

cd /d "%~dp0"

call "deploy_to_raspberry_pi.bat"
if errorlevel 1 (
    exit /b 1
)

call "run_raspberry_pi_game.bat"
if errorlevel 1 (
    exit /b 1
)
