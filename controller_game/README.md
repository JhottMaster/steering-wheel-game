# Steering Wheel Controller Game

Shared `raylib` game code for both the Windows desktop dev loop and the Raspberry Pi console build.

## What It Does

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
- `A`: toggle auto-drive / button-throttle mode in the game
- `P`: use `pitch` for steering
- `R`: use `roll` for debug comparison
- `Y`: use `yaw` / `heading` for steering
- `SPACE`: set the current sensor orientation as center
- `A` / `D` or left / right arrows: keyboard steering fallback input
- `W` / up arrow: keyboard acceleration fallback in button-throttle mode
- `S` / down arrow: keyboard brake fallback in button-throttle mode
- `F11`: toggle fullscreen on desktop builds
- `ESC`: quit

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
- launches the new Pi build in the background if the build succeeds

The stop script only runs the remote process stop step, which is useful when the Pi game is already running and you want the display back.

Create a local `raspberry_pi_config.bat` from `raspberry_pi_config.example.bat` and fill in your Pi host, username, and remote path first. Absolute Linux paths are the safest choice for `PI_REMOTE_DIR` and `PI_RAYLIB_DIR`.
