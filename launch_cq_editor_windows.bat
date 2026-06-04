@echo off
setlocal

cd /d "%~dp0"

if not exist ".venv\Scripts\CQ-editor.exe" (
    echo CQ-editor is not installed in .venv.
    echo Run:
    echo   .\.venv\Scripts\python -m pip install cq-editor
    exit /b 1
)

if not exist ".local-config\spyder" mkdir ".local-config\spyder"
if not exist ".local-appdata" mkdir ".local-appdata"
if not exist ".local-cache" mkdir ".local-cache"

set "SPYDER_CONFDIR=%CD%\.local-config\spyder"
set "LOCALAPPDATA=%CD%\.local-appdata"
set "XDG_CACHE_HOME=%CD%\.local-cache"

echo Launching CQ-editor with project-local config/cache folders...
".venv\Scripts\CQ-editor.exe" toddler_steering_wheel.py
