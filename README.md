# Toddler Steering Wheel

This repo contains the CAD work, firmware, and game experiments for the toddler steering wheel project.

## Main Parts

- [`controller_game`](controller_game): the shared `raylib` game code for both Windows desktop development and Raspberry Pi console deployment
- [`firmware`](firmware): microcontroller sketches for streaming steering data
- [`docs`](docs): project notes and setup docs
- [`toddler_steering_wheel.py`](toddler_steering_wheel.py): CADQuery model for printed parts

## Shared Game Workflow

The game now lives in [`controller_game`](controller_game) and is configured for:

- Raspberry Pi OS `Lite`
- `raylib` built with `PLATFORM_DRM`
- fixed `1920x1080` output
- `UDP` controller input on port `4210`
- the existing Windows `MSYS2` desktop build loop

Quick path on the Pi:

```bash
sudo apt update
sudo apt install -y git build-essential \
  libdrm-dev libegl1-mesa-dev libgles2-mesa-dev libgbm-dev libasound2-dev

git clone --depth 1 https://github.com/raysan5/raylib.git
cd raylib/src
make PLATFORM=PLATFORM_DRM

cd /path/to/steering-wheel-game/controller_game
make -f Makefile.raspberry_pi
./build/bin/steering_wheel_console
```

For Windows desktop work, run:

```text
controller_game\windows_build.bat
controller_game\windows_test.bat
```

For Windows-to-Pi remote deploys, use:

```text
controller_game\deploy_to_raspberry_pi.bat
```

More detail is in [`controller_game/README.md`](controller_game/README.md).
