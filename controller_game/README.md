# Steering Wheel Controller Game

Shared `raylib` game code for both the Windows desktop dev loop and the Raspberry Pi console build.

`src/main.cpp` owns the app setup and frame loop so it remains a useful starting point. Most modules keep their implementation in the header to make browsing the game easier. The rest of `src/` is organized by responsibility:

- `src/game/`: Road Carpet Drive logic, assets, audio, and game rendering
- `src/input/`: shared controller packet polling and packet-to-`SensorFrame` parsing behavior
- `src/views/`: secondary screens such as the hardware test dashboard
- `src/platform/`: Windows and Linux/Raspberry Pi networking, local IP lookup, and window defaults

This keeps networking, local IP discovery, window defaults, and small `raylib` API differences out of gameplay code.

Project structure preferences are captured in [`DEVELOPMENT_GUIDELINES.md`](DEVELOPMENT_GUIDELINES.md). In short: prefer header-oriented modules for this small game, keep `main.cpp` readable enough to understand the frame loop, and keep platform-specific socket/window details behind `src/platform/`.

## What It Does

- listens on `UDP` port `4210`
- expects packets like:

```text
qw=0.996,qx=0.012,qy=-0.084,qz=0.018,button1=0,button2=1
```

- the current controller firmware sends on input change and also forces a heartbeat packet at least every `200ms`
- on startup, the game immediately advertises itself over `UDP` broadcast on port `4211` so a controller can discover it
- once a controller is connected, the game stops broadcasting and only resumes if packets go stale for more than `5` seconds
- while rediscovering, the game sends a discovery beacon every `3` seconds and shows a disconnected banner in the game view

- defaults to the Road Carpet Drive game
- press `T` to show the hardware test dashboard
- drives a toy car around a generated 1024x1024 road-carpet map
- uses generated sprites from `assets/sprites`
- collects coin markers with a generous pickup radius
- renders a steering wheel from quaternion-derived twist around the selected sensor axis
- displays the latest quaternion, derived Euler readout, and two button states from the controller
- shows `button2` as the red left lamp and `button1` as the green right lamp in test mode
- falls back to keyboard input if no recent packets arrive
- desktop builds now default to a `1024x768` window to match the Raspberry Pi target layout more closely

## Controls

- `T`: toggle Road Carpet Drive / hardware test dashboard
- `A`: toggle auto-drive / button-throttle mode in the game
- in button-throttle mode, `button1` / `W` accelerates
- in button-throttle mode, `button2` / `S` brakes to a stop first and only engages reverse after the car has remained stopped for about `1` second
- `P`: use the sensor pitch-axis twist for steering
- `R`: use the sensor roll-axis twist for debug comparison
- `Y`: use the sensor yaw-axis twist for debug comparison
- `SPACE`: set the current sensor orientation as center
- controller recenter gesture: hold green (`button1`) and press red (`button2`) `3` times within `1.5` seconds
- `A` / `D` or left / right arrows: keyboard steering fallback input
- `W` / up arrow: keyboard acceleration fallback in button-throttle mode
- `S` / down arrow: keyboard brake fallback in button-throttle mode
- `F11`: toggle fullscreen on desktop builds
- `ESC`: quit

## Steering Notes

- gameplay steering uses quaternion-derived twist from the controller, not raw Euler steering angles
- the game tracks accumulated wheel rotation up to a `270` degree full-lock range instead of flipping direction after a half turn
- a small deadzone and blended response curve make it less twitchy near center while still giving stronger steering early in the turn
- reverse steering is automatically inverted so backing up behaves like a real car

## Assets

Generated game assets live in:

```text
assets/sprites/
assets/sounds/
```

Fixed filenames are used so the art can be replaced later without code changes:

- `road_carpet_map_2.png`
- `sports_car_top.png`
- `coin.png`

`toy_car_top.png` is kept as an alternate car sprite.

Sound assets are also generated original files:

- `carpet_cruise_loop.wav`
- `toy_engine_loop.wav`
- `coin_chime.wav`

To regenerate the current original art and sounds:

```text
python tools/generate_assets.py
python tools/generate_sounds.py
```

## Build And Run

### Windows Desktop Loop

This keeps the same fast local workflow:

```text
windows_build.bat
windows_test.bat
```

The Windows build first checks for a `raylib` checkout at `..\raylib`. If that is missing, it falls back to the MSYS2 MINGW64 raylib package, or you can set `RAYLIB_DIR`.

The Windows executable is statically linked against `libraylib.a` when available, so double-clicking `build\bin\steering_wheel_game.exe` does not require `libraylib.dll` beside it.

### Raspberry Pi Console Build

Build `raylib` for `PLATFORM_DRM`, then from this folder run:

```bash
make -f Makefile.raspberry_pi
./build/bin/steering_wheel_console
```

By default the Pi build expects `raylib` at `~/raylib`. Override that with:

```bash
make -f Makefile.raspberry_pi RAYLIB_DIR=/path/to/raylib
```

This Pi build uses the same gameplay code, but switches to a fixed `1920x1080` console-style setup.

The Raspberry Pi makefile links `-latomic` because the Pi 3 toolchain may need it for raylib's audio code.

## Remote Pi Deploy Loop

For Windows-to-Pi development there is also:

```text
deploy_to_raspberry_pi.bat
stop_raspberry_pi_game.bat
```

The deploy script:

- syncs the shared game files to the Pi over `ssh` / `scp`
- runs `Makefile.raspberry_pi` remotely and streams build errors back to your terminal
- stops the previous game process if one is running
- launches the new Pi build detached if the build succeeds
- writes runtime output to `/tmp/steering_wheel_console.log`

The stop script only runs the remote process stop step, which is useful when the Pi game is already running and you want the display back.

Create a local `raspberry_pi_config.bat` from `raspberry_pi_config.example.bat` and fill in your Pi host, username, remote path, and raylib path first:

```text
set "PI_HOST=raspberry-pi-game"
set "PI_USER=pablo"
set "PI_REMOTE_DIR=/home/pablo/steering-wheel-game/controller_game"
set "PI_RAYLIB_DIR=/home/pablo/raylib"
```

Absolute Linux paths are the safest choice for `PI_REMOTE_DIR` and `PI_RAYLIB_DIR`. SSH keys are recommended so deploys do not prompt for a password on every `ssh` / `scp` step.
