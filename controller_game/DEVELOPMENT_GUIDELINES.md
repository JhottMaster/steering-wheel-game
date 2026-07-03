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
- Keep Raspberry Pi deployment isolated to `deploy_to_raspberry_pi.bat`, `stop_raspberry_pi_game.bat`, and `Makefile.raspberry_pi`.
- When adding source folders that the Pi needs, update the deploy package list.
- The Pi build targets `1920x1080` and raylib `PLATFORM_DRM`.
