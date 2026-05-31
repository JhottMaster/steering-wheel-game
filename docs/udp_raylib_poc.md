# UDP Raylib POC

This proof of concept connects the current hardware bring-up to a very small cross-platform game shell:

- `XIAO ESP32-C3`
- `BNO055`
- `Wi-Fi UDP`
- `raylib` desktop app on Windows or Linux

## Goal

Show a centered 2D bar on screen that follows either the `roll` or `pitch` reported by the BNO055.

## Repo Layout

- [controller_poc/src/main.cpp](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\src\main.cpp)
- [controller_poc/Makefile](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\Makefile)
- [controller_poc/windows_build.bat](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\windows_build.bat)
- [firmware/xiao_bno055_udp_sender/xiao_bno055_udp_sender.ino](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\firmware\xiao_bno055_udp_sender\xiao_bno055_udp_sender.ino)

## Data Path

1. The XIAO reads fused orientation data from the BNO055.
2. The XIAO sends packets over Wi-Fi using `UDP`.
3. The `raylib` app listens on port `4210`.
4. The app moves a horizontal bar based on incoming `roll` or `pitch`.

Packet format:

```text
roll=12.4,pitch=-3.1,heading=182.0
```

The current working firmware only sends when the orientation changes enough to matter, rather than streaming every frame while stationary.

## Firmware Setup

The tracked firmware sketch uses a local secrets header:

- tracked template: `firmware/xiao_bno055_udp_sender/wifi_secrets.example.h`
- local untracked file: `firmware/xiao_bno055_udp_sender/wifi_secrets.h`

Copy the example structure into the local header and fill in:

```text
REPLACE_WITH_WIFI_SSID
REPLACE_WITH_WIFI_PASSWORD
REPLACE_WITH_HOST_IP
```

`REPLACE_WITH_HOST_IP` should be the IP address of the Windows or Linux machine running the `raylib` app.

Important note:

- attach the XIAO's external Wi-Fi / BLE antenna before expecting reliable wireless behavior
- the firmware sets a friendlier Wi-Fi hostname: `steering-wheel-poc-esp32c3`
- the serial monitor prints explicit `UDP #... sent to ...` lines for successful sends

## Desktop App Behavior

- `R` displays `roll`
- `P` displays `pitch`
- `SPACE` centers the current value
- `A` / `D` or left / right arrows provide a keyboard fallback when packets are not arriving
- the app shows the host IPv4 address in the window so it is easy to copy into `wifi_secrets.h`
- the app keeps the last received value on screen even if packets go stale

## Build Notes

### Windows

The POC expects a `raylib` checkout at `controller_poc/../raylib` by default, similar to your existing workflow. If needed, set `RAYLIB_DIR` before building.

### Linux

The POC expects a system `raylib` package discoverable through `pkg-config`.

## Suggested First Test

1. Build and launch the `raylib` app.
2. Confirm the keyboard fallback moves the bar.
3. Fill in Wi-Fi credentials and host IP in `wifi_secrets.h`.
4. Upload the firmware.
5. Confirm the app switches from keyboard fallback to live `UDP` sensor input.

## Real-World Networking Notes

This was verified working on a network where the Windows PC and XIAO were on different VLANs / network segments.

What mattered in practice:

- Windows needed an inbound firewall allow rule for the app or UDP port `4210`
- the router also needed an allow rule for UDP traffic between the relevant network segments
- local loopback testing from PowerShell to the app was useful to separate app issues from network issues
