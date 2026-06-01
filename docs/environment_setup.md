# Environment Setup

This repo is used across multiple machines and operating systems. These are the working commands and notes for the current setup.

## Windows 11

### CadQuery CLI

Create and use the local virtual environment:

```powershell
python -m venv .venv
.\.venv\Scripts\python -m pip install cadquery
```

Run the CAD script:

```powershell
.\.venv\Scripts\python toddler_steering_wheel.py
```

This exports:

- `toddler_steering_wheel.step`
- `toddler_steering_wheel.stl`
- `toddler_steering_wheel_cartridge.step`
- `toddler_steering_wheel_cartridge.stl`

Important note:

- On this Windows setup, CadQuery/OCP was crashing during native shutdown after successful exports. The script now uses a CLI-only hard exit after export so command-line runs finish cleanly while CQ-editor behavior remains intact.

### CQ-editor on Windows

Install into the same virtual environment:

```powershell
.\.venv\Scripts\python -m pip install cq-editor
```

Launch with the project-local wrapper:

```powershell
.\launch_cq_editor_windows.bat
```

Why the wrapper exists:

- CQ-editor / Spyder wanted to write config under `C:\Users\Pablo\.spyder-py3`
- `ezdxf` wanted to write cache under `C:\Users\Pablo\.cache`
- this repo now uses local folders instead:
  - `.local-config\spyder`
  - `.local-cache`

## macOS

The earlier working environment used `uv` plus CQ-editor.

Typical setup:

```bash
uv venv
source .venv/bin/activate
uv pip install cadquery==2.7.0 cq-editor==0.7.0
```

Run exports:

```bash
.venv/bin/python toddler_steering_wheel.py
```

Open in CQ-editor:

```bash
QT_ENABLE_HIGHDPI_SCALING=0 .venv/bin/cq-editor toddler_steering_wheel.py
```

The HiDPI environment variable was important on macOS because CQ-editor rendered incorrectly without it on the previous machine.

## Repo Notes

- `firmware/xiao_bno055_udp_sender/wifi_secrets.h` is intentionally local-only and ignored by git.
- `.venv/`, `.local-config/`, and `.local-cache/` are also local-only and ignored.
- The current CAD model now exports both the wheel and a separate removable cartridge part.
