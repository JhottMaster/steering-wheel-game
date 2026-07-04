# Steering Wheel Controller Game

Shared `raylib` game code for both the Windows desktop dev loop and the Raspberry Pi console build.

`src/main.cpp` owns the app setup and frame loop so it remains a useful starting point. Most modules keep their implementation in the header to make browsing the game easier. The rest of `src/` is organized by responsibility:

- `src/game/`: Road Carpet Drive logic, assets, audio, and game rendering
- `src/app/`: shared app-level state such as app mode, logical controller buttons, and pause menu behavior
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
- starts paused so a controller-only Raspberry Pi boot lands on a usable menu before gameplay begins
- press `T` to show the hardware test dashboard
- drives a toy car around a spreadsheet-authored toy-carpet city map
- uses generated sprites from `assets/sprites`
- draws roads, buildings, trees, bushes, coins, and spawn markers from `assets/cities/demo_city.csv`
- the current demo city has `32` collectible coins and `34` building props
- uses the CSV city data for spawn position, coin pickup, road/off-road behavior, and obstacle collision
- starts in button-throttle mode by default, with the car stopped until you accelerate
- gives button-throttle mode a higher top speed than auto-drive
- kicks up fading dust puffs and clamps speed harder when the car leaves the road
- adds animated kraken road encounters that wobble, growl when approached, and sink away when collected
- renders a steering wheel from quaternion-derived twist around the selected sensor axis
- displays the latest quaternion, derived Euler readout, and two button states from the controller
- shows `button2` as the red left lamp and `button1` as the green right lamp in test mode
- falls back to keyboard input if no recent packets arrive
- keeps the in-game HUD minimal: coins and speed stay visible, while diagnostics such as IP, FPS, and zoom live in the pause menu
- logs lightweight frame timing diagnostics to the terminal every `2` seconds while running
- desktop builds now default to a `1024x768` window to match the Raspberry Pi target layout more closely

## Controls

- `T`: toggle Road Carpet Drive / hardware test dashboard
- `A`: toggle auto-drive / button-throttle mode in the game
- in button-throttle mode, `button1` / up arrow accelerates
- in button-throttle mode, `button2` / down arrow brakes to a stop first and only engages reverse after the car has remained stopped for about `1` second
- `P`: use the sensor pitch-axis twist for steering
- `R`: use the sensor roll-axis twist for debug comparison
- `Y`: use the sensor yaw-axis twist for debug comparison
- `SPACE`: set the current sensor orientation as center
- controller recenter gesture: hold green (`button1`) and press red (`button2`) `3` times within `1.5` seconds
- controller pause gesture: hold green + red together for `3` seconds
- pause menu: steer left/right to choose, green selects, red resumes/goes back
- keyboard testing: up arrow also acts like green, down arrow also acts like red
- pause menu options: resume, restart, center wheel, quit, toggle acceleration mode, hardware test/back to game, zoom out, and zoom in
- left / right arrows: keyboard steering fallback input that automatically returns to center when released
- up arrow: desktop accelerate input in button-throttle mode
- down arrow: desktop brake/reverse input in button-throttle mode
- `-` / `=`: zoom the game camera out / in while tuning the city
- `0`: reset the game camera zoom
- `E`: toggle road art editor mode for tuning one hard-coded road piece and showing collision outlines
- in road art editor mode, `` ` `` / `~` toggles whether up/down selects the asset type or value field
- in road art editor field selection, up/down selects `scale_percent`, `footprint_percent`, `offset_x`, `offset_y`, `anchor_x`, or `anchor_y`
- in road art editor asset selection, up/down selects the road piece to tune
- in road art editor mode, left/right adjusts the selected value by `1` and auto-saves the config
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
assets/cities/
assets/sprites/
assets/sounds/
```

The current visual city is defined by:

```text
assets/cities/demo_city.csv
```

The carpet background is tiled from `terrain_carpet_tilemirror.png`; roads, props, buildings, coins, and the car are separate sprites drawn on top.

Fixed sprite filenames are used so the art can be replaced later without changing map files:

- `terrain_carpet_tilemirror.png`
- `road_horizontal_repeat.png`
- `road_vertical_repeat.png`
- `road_intersection_4way_crosswalks.png`
- `road_curve_bottom_right.png`
- `road_curve_bottom_left.png`
- `road_curve_top_right.png`
- `road_curve_top_left.png`
- `coin_star.png`
- `kraken_octopus_pop_sheet.png`
- `kraken_octopus_road_sheet.png`
- `toy_sports_car.png`
- `toy_car_tire.png`
- building sprites such as `building_house.png`, `building_shop.png`, and `building_fire_station.png`
- prop sprites such as `prop_bush_cluster.png`, `prop_evergreen.png`, and `prop_tree_round_ai_01.png`

Road curve filenames should match their semantic direction. For example, `road_curve_bottom_left.png`
is the art used by `CitySprite::kRoadCurveBottomLeft` and the `r_bl` city token.

Sound assets include:

- `carpet_cruise_loop.wav`
- `engine_large.ogg`, falling back to `toy_engine_loop.wav`
- `coin_chime.wav`
- `silent_beast_growl.mp3`: plays when an uncollected kraken first comes near the car
- `splash1.wav`: plays when a kraken is collected and sinks away
- optional fallback names `kraken_growl.wav` and `kraken_sink.wav` are also supported

To regenerate the current original art and sounds:

```text
python tools/generate_sounds.py
```

To regenerate simple procedural placeholder art that uses the same filenames loaded by the game:

```text
python tools/generate_assets.py
```

Some art helper scripts live in `tools/`, including tile mirroring and road alpha processing.

## City CSV Format

The city map uses a spreadsheet-friendly CSV where each row is map `Y`, each column is map `X`, and each cell contains a pipe-separated list of things to draw in that tile.

Blank cells mean "just draw the tiled carpet background." A cell can contain one road tile plus any number of visual objects:

```csv
,,,,r_br,r_h|coin:star,r_h,r_bl
,house@256:280,,,r_v|coin:star
```

Cell contents use this shape:

```text
token
token@x:y
token@x:y*scale
token|token@x:y|token
```

Offsets are local tile pixels in a `512 x 512` authoring coordinate space, so `@256:256` means the center of a tile. The game currently renders each authored tile as `256 x 256` world pixels, which keeps the spreadsheet readable while making the city more compact. If an object has no offset, the parser places it at the tile center.

Current road tokens:

- `r_h`: horizontal road
- `r_v`: vertical road
- `r_x`: 4-way intersection
- `r_br`: curve connecting bottom and right
- `r_bl`: curve connecting bottom and left
- `r_tr`: curve connecting top and right
- `r_tl`: curve connecting top and left

Current object tokens:

- `coin:star`
- `kraken:pop`
- `kraken:road`
- `spawn:player`
- `tree:round`
- `tree:evergreen`
- `bush`
- `house`
- `shop`
- `school`
- `police`
- `fire_station`
- `library`

The parser for this format is in `src/game/city_map.h`. The same parsed city data drives rendering and gameplay:

- road tokens create drivable road shapes
- coin tokens create collectible coins
- kraken tokens create animated road encounters that pop/fade away when the car drives through them
- the HUD shows collected krakens separately from coins
- `spawn:player` sets the starting car position
- trees, bushes, and buildings create approximate obstacle collision shapes

Collision is intentionally based on authored geometry, not sprite alpha pixels. That keeps gameplay stable even as art is resized, feathered, or replaced.

Road art placement is tuned separately from road collision. Per-piece visual scale,
footprint scale, `X/Y` offsets, anchor edges, and draw mode live in
`assets/config/road_art_tuning.csv`, so straight roads, each curve direction, and intersections can
be nudged independently while the CSV tokens and collision shapes stay stable. `scale_percent`
shrinks or grows the art within its drawing box; `footprint_percent` shrinks or grows that drawing
box relative to the authored tile and can go above `100` when a sprite, such as the 4-way
intersection, needs to draw outside its logical collision tile. Anchor values are stepped positions:
`-1`, `0`, and `1` mean top/left, center, and bottom/right, while values beyond that intentionally
push the drawing box farther outside the logical tile. The in-game road art editor toggled by `E` can edit any configured road
sprite, auto-saves this config as values change, and overlays the current gameplay collision
geometry. Cyan outlines are drivable road shapes from the CSV tokens, orange outlines are obstacle
collision shapes, and the pale circle is the car collision radius.

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

At startup, the Pi build now performs a small network preflight before entering the frame loop. If it
cannot find an active non-loopback IPv4 address or cannot open the UDP listener / broadcast socket, it
logs a `[startup] ...` message to stdout and exits early instead of taking over the screen with a game
session that cannot accept controller traffic.

Keyboard fallback is still useful for the Windows desktop loop, but it should currently be treated as
best-effort only on the Raspberry Pi `PLATFORM_DRM` build. The game code does poll raylib keyboard
state on Pi too, but in practice the console build may not receive normal key events reliably even
though kernel-level shortcuts such as `Ctrl` + `Alt` + `Del` still work.

## Remote Pi Deploy Loop

For Windows-to-Pi development, run these scripts from the real Windows shell in `controller_game/`:

```text
deploy_to_raspberry_pi.bat
run_raspberry_pi_game.bat
deploy_and_run_raspberry_pi.bat
stop_raspberry_pi_game.bat
```

In Codex, these scripts must run with approval/escalation so they use the real Windows `ssh`, `scp`, and `tar` tools. If they are run in the default sandbox, `ssh` may resolve to a deny shim instead of contacting the Pi.

The deploy script is the fast compile-feedback path. It:

- syncs the shared game files to the Pi over `ssh` / `scp`
- runs `Makefile.raspberry_pi` remotely and streams build errors back to your terminal

It intentionally does not stop or launch the game. That keeps most Pi iterations focused on whether the code compiles:

```text
deploy_to_raspberry_pi.bat
```

The run script:

- stops the previous game process if one is running
- launches the already-built Pi binary detached
- writes runtime output to `/tmp/steering_wheel_console.log`

Use it after a successful deploy/build when you want to update the Pi display:

```text
run_raspberry_pi_game.bat
```

Use the wrapper when you want the old one-command behavior:

```text
deploy_and_run_raspberry_pi.bat
```

The stop script only runs the remote process stop step, which is useful when the Pi game is already running and you want the display back.

For the persistent boot service on the Pi, the current stable setup is the plain `pablo` user service
plus a small drop-in override:

```ini
[Unit]
ConditionPathExists=!/boot/firmware/NO_GAME
ConditionPathExists=!/boot/NO_GAME

[Service]
SupplementaryGroups=input video render
ExecStartPre=/bin/sleep 5
```

That extra sleep gives Wi-Fi a moment to finish coming up during boot, and the `NO_GAME` sentinel file
provides an easy escape hatch if you ever need the Pi to boot to the console without launching the game.
Launching the service through `openvt` as root did attach it to `tty1`, but it was not worth the tradeoff,
so the recommended setup remains the non-root service above.

Create a local `raspberry_pi_config.bat` from `raspberry_pi_config.example.bat` and fill in your Pi host, username, remote path, and raylib path first:

```text
set "PI_HOST=raspberry-pi-game"
set "PI_USER=pablo"
set "PI_REMOTE_DIR=/home/pablo/steering-wheel-game/controller_game"
set "PI_RAYLIB_DIR=/home/pablo/raylib"
```

Absolute Linux paths are the safest choice for `PI_REMOTE_DIR` and `PI_RAYLIB_DIR`. SSH keys are recommended so deploys do not prompt for a password on every `ssh` / `scp` step.

On a 1 GB Raspberry Pi 3 B, compiling while the game is running may be tight on memory. If the remote build is terminated unexpectedly, stop the game first and rerun the build-only deploy:

```text
stop_raspberry_pi_game.bat
deploy_to_raspberry_pi.bat
```
