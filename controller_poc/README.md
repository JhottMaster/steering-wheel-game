# Steering Wheel Controller POC

Minimal `raylib` app that listens for steering sensor data over `UDP` and visualizes it as a horizontal bar.

## What It Does

- opens a simple 2D window
- listens on `UDP` port `4210`
- expects packets like:

```text
roll=12.4,pitch=-3.1,heading=182.0
```

- renders a centered slider that tracks either `roll` or `pitch`
- falls back to keyboard input if no recent packets arrive

## Controls

- `R`: show `roll`
- `P`: show `pitch`
- `SPACE`: set the current position as center
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
