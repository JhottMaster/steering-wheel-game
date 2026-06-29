# Steering Wheel Controller POC

Minimal `raylib` app that listens for steering sensor data over `UDP` and visualizes it as a rotating steering wheel.

## What It Does

- opens a simple 2D window
- listens on `UDP` port `4210`
- expects packets like:

```text
roll=12.4,pitch=-3.1,heading=182.0,button1=0,button2=1
```

- defaults to a blank game placeholder screen
- press `T` to show the hardware test dashboard
- renders a steering wheel that turns with `pitch` by default in test mode
- displays the latest `roll`, `pitch`, `heading`, and two button states from the controller
- shows `button2` as the red left lamp and `button1` as the green right lamp in test mode
- falls back to keyboard input if no recent packets arrive

## Controls

- `T`: toggle game placeholder / hardware test dashboard
- `P`: use `pitch` for steering
- `R`: use `roll` for debug comparison
- `Y`: use `yaw` / `heading` for steering
- `SPACE`: set the current sensor orientation as center
- `A` / `D` or left / right arrows: keyboard fallback input

## Building

### Linux

Install `raylib` development files so `pkg-config` can find them, then run:

```bash
make buildonly
./build/bin/steering_wheel_poc
```

### Windows

This project follows the same rough pattern as your existing Raylib setup:

- install `MSYS2` with the `mingw64` toolchain
- provide a `raylib` checkout at `../raylib`, or set `RAYLIB_DIR`

Then run:

```text
windows_build.bat
```

## Notes

- This is intentionally a proof of concept, not a finished game architecture.
- The goal is to prove that sensor orientation can drive a cross-platform `raylib` executable cleanly.
