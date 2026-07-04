# Controller Game Development Guidelines

These notes capture the project shape we want to preserve while iterating quickly on the Windows desktop build and Raspberry Pi console build.

## Source Layout

- Keep `src/main.cpp` as the readable app entry point. It should own setup, the frame loop, mode switching, high-level input flow, update calls, and draw calls.
- Do not shrink `main.cpp` into a tiny launcher that only jumps to another file. Opening it should explain what the game does each frame.
- Keep gameplay, rendering helpers, input behavior, and platform code organized under the existing `src/` folders.
- Avoid scattering new tiny files unless the split makes navigation materially easier.

## Header-Oriented Modules

- Prefer modules where the header contains the implementation, especially for small game systems.
- Use a separate `.cpp` only when the distinction between interface and implementation adds real clarity or reduces meaningful compile/link pain.
- Mark header-defined functions `inline`.
- Put private helpers in a small `*_detail` namespace instead of an anonymous namespace, because multiple headers are included into the same translation unit.

## Platform Boundaries

- Keep platform-specific code in `src/platform/`.
- Platform files should handle OS details such as sockets, nonblocking datagram reads, local IP lookup, window defaults, and small raylib API differences.
- Shared behavior should stay outside platform files. For example, platform receivers expose one datagram at a time, while `src/input/sensor_receiver.h` owns the shared "drain packets and keep the latest valid `SensorFrame`" behavior.
- Avoid peppering gameplay files with `#ifdef`s. Prefer selecting the platform header once near the boundary.

## Build And Deploy

- Keep the Windows loop fast: `windows_build.bat` and `windows_test.bat` should remain the first verification path.
- Keep Raspberry Pi deployment isolated to the Pi scripts and `Makefile.raspberry_pi`.
- `deploy_to_raspberry_pi.bat` is the build-feedback path: sync and compile only.
- `run_raspberry_pi_game.bat` launches the already-built remote binary.
- `deploy_and_run_raspberry_pi.bat` is only the convenience wrapper when the display should update immediately.
- `stop_raspberry_pi_game.bat` should remain safe to run independently before memory-heavy Pi builds.
- When adding source folders that the Pi needs, update the deploy package list.
- The Pi build targets `1920x1080` and raylib `PLATFORM_DRM`.
- In Codex, SSH-based scripts need approval/escalation so they run with the real Windows OpenSSH tools instead of the sandbox deny shim.

## City Map Format

- Keep the authorable city map in `assets/cities/demo_city.csv`.
- Treat the CSV as a visual grid: row is map `Y`, column is map `X`, and each cell is a pipe-separated stack of tokens.
- Empty cells mean plain tiled carpet background; do not require a `grass` token.
- Prefer compact readable map tokens such as `r_h`, `r_v`, `r_x`, `coin:star`, and `tree:round@x:y`.
- Use the parsed city data for gameplay semantics; avoid runtime collision behavior based on rendered sprite alpha pixels.
- Roads should own simple drivable geometry, and props/buildings should own approximate collision shapes tuned for feel.
- Keep road visual tuning first-class and separate from gameplay geometry. Per-piece road art scale, footprint scale, offsets, anchor edges, and draw modes belong in `assets/config/road_art_tuning.csv`, with parsing/editor behavior in `src/game/road_art_tuning.h`.
- Keep road curve asset filenames aligned with their semantic enum/token direction; `road_curve_bottom_left.png` should be the asset loaded for `CitySprite::kRoadCurveBottomLeft` / `r_bl`.
