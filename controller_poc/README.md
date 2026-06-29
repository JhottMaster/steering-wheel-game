# Steering Wheel Controller POC

Minimal `raylib` app that listens for steering sensor data over `UDP` and runs a simple top-down Road Carpet Drive game.

## What It Does

- opens a simple 2D window
- listens on `UDP` port `4210`
- expects packets like:

```text
roll=12.4,pitch=-3.1,heading=182.0,button1=0,button2=1
```

- defaults to the Road Carpet Drive game
- press `T` to show the hardware test dashboard
- drives a toy car around a generated 1024x1024 road-carpet map
- uses generated sprites from `assets/sprites`
- collects coin markers with a generous pickup radius
- renders a steering wheel that turns with `pitch` by default in test mode
- displays the latest `roll`, `pitch`, `heading`, and two button states from the controller
- shows `button2` as the red left lamp and `button1` as the green right lamp in test mode
- falls back to keyboard input if no recent packets arrive

## Controls

- `T`: toggle Road Carpet Drive / hardware test dashboard
- `F11`: toggle fullscreen
- `A`: toggle auto-drive / button-throttle mode in the game
- `P`: use `pitch` for steering
- `R`: use `roll` for debug comparison
- `Y`: use `yaw` / `heading` for steering
- `SPACE`: set the current sensor orientation as center
- `A` / `D` or left / right arrows: keyboard steering fallback input
- `W` / up arrow: keyboard acceleration fallback in button-throttle mode
- `S` / down arrow: keyboard brake fallback in button-throttle mode

## Assets

Generated game assets live in:

```text
assets/sprites/
```

Fixed filenames are used so the art can be replaced later without code changes:

- `road_carpet_map_2.png`
- `sports_car_top.png`
- `coin.png`

`toy_car_top.png` is kept as an alternate car sprite.

To regenerate the current original art:

```text
python tools/generate_assets.py
```

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

Run logic tests on Windows with:

```text
windows_test.bat
```

On Linux, run:

```bash
make test
```

## Notes

- This is intentionally a proof of concept, not a finished game architecture.
- The goal is to prove that sensor orientation can drive a cross-platform `raylib` executable cleanly.
