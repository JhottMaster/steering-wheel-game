# Toddler Steering Wheel Game - Codex Handoff

## Project Goal

Build a small handheld steering wheel toy for a toddler. The physical wheel should be 3D printable and eventually contain an ESP32-class microcontroller, an IMU/gyro sensor, and a battery/power source. The longer-term idea is to use the wheel as a game controller, likely sending steering/tilt data to a browser game or desktop app over BLE or Wi-Fi.

CadQuery is the source of truth for the CAD model. The model should stay parametric, with important dimensions declared as variables near the top of the Python file.

## Repo And Location

Local project folder:

```text
/Users/pabloaizpiri/dev/cad/toddler-steering-wheel
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

The project has a local Python virtual environment created with `uv`:

```text
.venv
```

Installed packages include:

```text
cadquery==2.7.0
cq-editor==0.7.0
```

`uv` was used because `conda`/`mamba` were not visible from the Codex shell, while `uv` was available and already had a Python 3.12 install.

## Useful Commands

Regenerate STEP and STL:

```bash
cd ~/dev/cad/toddler-steering-wheel
.venv/bin/python toddler_steering_wheel.py
```

Open in CQ-editor:

```bash
cd ~/dev/cad/toddler-steering-wheel
QT_ENABLE_HIGHDPI_SCALING=0 .venv/bin/cq-editor toddler_steering_wheel.py
```

A convenience shortcut was created at:

```text
/Users/pabloaizpiri/.local/bin/cqeditor
```

So this should open the model with the macOS HiDPI workaround applied:

```bash
cqeditor
```

## CQ-editor Display Bug

CQ-editor had a macOS HiDPI/Retina scaling issue where the 3D viewport rendered only into a small part of the preview area. Launching with this environment variable fixed it:

```bash
QT_ENABLE_HIGHDPI_SCALING=0
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

The pocket size was a rough guess for a small ESP32/Arduino Nano-style board plus an IMU breakout. It is not yet based on a chosen board.

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
ESP32 or similar microcontroller
separate IMU/gyro module unless the chosen board has one built in
battery/power source still undecided
cover plate screwed into the hub
heat-set brass inserts for screws
```

Most plain ESP32 dev boards do not include a gyro/IMU. Some specialty boards do. Candidate IMU modules include MPU-6050, MPU-9250, ICM-20948, LSM6DS3, BMI270, or similar.

For the final electronics version, consider:

```text
two-piece hub with screw-on rear cover
heat-set insert pockets/standoffs
proper battery retention
secure cover screws
no exposed coin cells or loose small parts for toddler use
```

## Likely Next Steps

1. Print the current STL as a physical hand-feel prototype.
2. While it prints, inventory available electronics: ESP32 board model, IMU model, battery options, switches, charger boards, LEDs/buttons/displays.
3. Pick the communication approach:
   - BLE HID/gamepad if pretending to be a controller.
   - BLE UART or Wi-Fi/WebSocket if talking to a custom browser game.
4. Prototype firmware that reads IMU steering angle.
5. Prototype a tiny browser game or visualizer that displays steering input.
6. Once electronics are chosen, revise the CAD:
   - real rear pocket or two-piece cover
   - board standoffs
   - heat-set insert holes
   - switch/charging access
   - cable/programming access if needed

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
