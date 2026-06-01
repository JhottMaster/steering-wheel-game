# Toddler Steering Wheel Game - Codex Handoff

## Project Goal

Build a small handheld steering wheel toy for a toddler. The physical wheel should be 3D printable and eventually contain an ESP32-class microcontroller, an IMU/gyro sensor, and a battery/power source. The longer-term idea is to use the wheel as a game controller, likely sending steering/tilt data to a browser game or desktop app over BLE or Wi-Fi.

CadQuery is the source of truth for the CAD model. The model should stay parametric, with important dimensions declared as variables near the top of the Python file.

## Repo And Location

Local project folder:

```text
C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game
```

GitHub repo:

```text
git@github.com:JhottMaster/steering-wheel-game.git
```

Main CAD file:

```text
toddler_steering_wheel.py
```

Generated exports:

```text
toddler_steering_wheel.step
toddler_steering_wheel.stl
```

## Environment

The repo now has a working Windows local Python virtual environment:

```text
.venv
```

On Windows 11, confirmed working packages include:

```text
cadquery==2.5.2
cq-editor==0.5.0
```

There is also historical macOS setup context from the earlier machine where `uv` was used because `conda`/`mamba` were not visible from the Codex shell.

Cross-platform environment/setup notes now live here:

```text
docs/environment_setup.md
```

## Useful Commands

Windows 11 regenerate STEP and STL:

```powershell
.\.venv\Scripts\python toddler_steering_wheel.py
```

Windows 11 open in CQ-editor:

```powershell
.\launch_cq_editor_windows.bat
```

macOS regenerate STEP and STL:

```bash
.venv/bin/python toddler_steering_wheel.py
```

macOS open in CQ-editor:

```bash
QT_ENABLE_HIGHDPI_SCALING=0 .venv/bin/cq-editor toddler_steering_wheel.py
```

## CQ-editor Display Bug

CQ-editor had a macOS HiDPI/Retina scaling issue where the 3D viewport rendered only into a small part of the preview area. Launching with this environment variable fixed it:

```bash
QT_ENABLE_HIGHDPI_SCALING=0
```

On Windows 11, CQ-editor installed successfully in the repo-local `.venv`, but needed a wrapper launcher so Spyder config and `ezdxf` cache would use repo-local folders rather than user-home locations:

```text
launch_cq_editor_windows.bat
```

## Current CAD State

The current model is a simple steering wheel body meant as a first print to test size and hand feel.

Current important dimensions:

```text
wheel_outer_diameter = 180.0 mm
wheel_inner_diameter = 118.0 mm
wheel_thickness = 16.0 mm
hub_diameter = 54.0 mm
hub_thickness = 22.0 mm
spoke_count = 3
spoke_width = 20.0 mm
spoke_thickness = 14.0 mm
```

The wheel has rounded grip edges:

```text
grip_roundover_radius = 7.0 mm
hub_roundover_radius = 2.0 mm
spoke_roundover_radius = 2.0 mm
```

The original electronics pocket was likely to need support material when printing flat, so it is currently off:

```text
include_electronics_pocket = False
include_center_hole = False
include_mounting_holes = False
```

Instead, there is a shallow front inset to show the approximate electronics footprint without requiring supports:

```text
include_pocket_footprint_inset = True
pocket_width = 34.0 mm
pocket_height = 48.0 mm
pocket_footprint_inset_depth = 1.2 mm
```

The pocket size was a rough guess for a small ESP32/Arduino Nano-style board plus an IMU breakout. It is not yet based on a chosen board, but the first print confirmed that a real electronics cavity can likely fit a `XIAO ESP32-C3` and `BNO055` side by side if the current shallow hub area is made deeper.

## Printing Notes

Printer:

```text
Bambu Lab P1P
```

Build volume is large enough for this model. The wheel is currently about 180 mm across, fitting comfortably on the 256 mm bed.

Recommended first prototype:

```text
Print flat, face-up.
Use the current no-real-pocket STL.
Purpose: test toddler hand feel, grip size, and overall diameter.
```

The current shallow inset should not create the same support problem as the deeper rear electronics pocket.

## Electronics Discussion So Far

Likely components:

```text
Seeed Studio XIAO ESP32-C3
Adafruit BNO055 absolute orientation sensor
battery/power source still undecided
cover plate screwed into the hub
heat-set brass inserts for screws
```

Other parts found in the parts bin during hardware inventory:

```text
generic ESP32 board
Adafruit ItsyBitsy 48 MHz 3.3V
Adafruit Bluetooth Feather
Arduino Micro
Meadow board
Adafruit LIS3DH 3-axis accelerometer
LC Studio MMA7361-style 3-axis accelerometer board
```

Reasoning behind the current preferred stack:

```text
XIAO ESP32-C3 is very small and has USB-C for simple first-prototype power and programming.
BNO055 includes fused orientation and is the strongest sensor found so far for steering input.
LIS3DH and MMA7361 are usable fallbacks for rough tilt input but are accelerometer-only.
Arduino Micro remains a possible wired USB HID option later if pretending to be a keyboard/gamepad becomes the simplest Raspberry Pi integration path.
```

For the final electronics version, consider:

```text
two-piece hub with screw-on rear cover
heat-set insert pockets/standoffs
proper battery retention
secure cover screws
no exposed coin cells or loose small parts for toddler use
```

## Validated Bring-Up Status

As of `2026-05-31`, the following combination has been tested successfully over `USB-C`:

```text
Seeed Studio XIAO ESP32-C3
Adafruit BNO055
I2C connection
USB serial output from a simple Arduino sketch
```

This de-risks the most important early electronics question: the current preferred controller and sensor stack works well enough to proceed with a real prototype.

The successful smoke-test sketch and setup notes are now in the repo:

```text
docs/xiao_bno055_bringup.md
firmware/xiao_bno055_smoketest/xiao_bno055_smoketest.ino
```

There is also now a very small cross-platform game/input proof of concept in the repo:

```text
controller_poc/
docs/udp_raylib_poc.md
firmware/xiao_bno055_udp_sender/xiao_bno055_udp_sender.ino
```

Current test wiring:

```text
XIAO 3V3 -> BNO055 VIN
XIAO GND -> BNO055 GND
XIAO D4/SDA -> BNO055 SDA
XIAO D5/SCL -> BNO055 SCL
```

Current first-prototype power strategy:

```text
Use USB-C to power and program the XIAO.
Do not block on battery selection before the first real electronics prototype.
Expose the XIAO USB-C port in the CAD revision.
```

Additional bring-up notes:

```text
The XIAO ESP32-C3 does not expose a normal programmable onboard user LED.
The tiny red onboard light appears to be a power LED, not a general status LED to build around.
The smoke-test sketch includes a short quiet startup delay after reset to make reprogramming smoother.
```

## Validated Wi-Fi POC Status

As of `2026-05-31`, the Wi-Fi POC is also working end-to-end:

```text
XIAO ESP32-C3 reads orientation from BNO055
XIAO sends UDP packets over Wi-Fi
Raylib desktop app receives packets and displays a steering bar
Windows 11 test machine verified
```

Important practical notes from the working setup:

```text
The external XIAO antenna must be attached or Wi-Fi signal can be unusably weak.
The firmware uses a local untracked wifi_secrets.h file plus a tracked wifi_secrets.example.h template.
The XIAO now advertises a friendlier hostname: steering-wheel-poc-esp32c3
The Raylib app can display the host IPv4 address to make host IP setup easier.
The Windows app can receive local UDP test packets from PowerShell.
To get real XIAO-to-PC traffic working, both Windows firewall and router/VLAN UDP rules mattered.
```

## Likely Next Steps

1. Keep the current `XIAO + BNO055` wiring intact while it is known-good.
2. Refine the signal the game uses:
   - derive a more intentional steering value from roll/pitch
   - add better centering / calibration behavior
   - decide whether heading should be ignored for gameplay
3. Decide whether the first real game path stays on Wi-Fi UDP or also needs a wired USB fallback.
4. Revise the CAD around the real prototype electronics:
   - deeper hub cavity
   - XIAO and BNO055 placement
   - USB-C access slot
   - likely rear cover strategy
5. After the wired or Wi-Fi prototype works, revisit battery, charger, switch, and mounting hardware choices.

## Style Preference For Future CAD

Generate CadQuery Python scripts with:

```text
clear variables at top
CadQuery Workplane API
show_object(result)
STEP and STL exports
comments for key geometry blocks
simple geometry first, complexity only when needed
```

Prefer CadQuery over OpenSCAD. Keep CadQuery as the source of truth.
