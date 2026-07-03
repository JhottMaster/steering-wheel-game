# UDP Raylib POC

This proof of concept connects the current hardware bring-up to a very small cross-platform game shell:

- `XIAO ESP32-C3`
- `BNO055`
- `Wi-Fi UDP`
- `raylib` desktop app on Windows or Linux

## Goal

Show a simple top-down Road Carpet Drive game by default, with a `T`-toggle hardware test dashboard that visualizes BNO055 orientation and button input.

## Repo Layout

- [controller_poc/src/main.cpp](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\src\main.cpp)
- [controller_poc/Makefile](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\Makefile)
- [controller_poc/windows_build.bat](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\windows_build.bat)
- [firmware/xiao_bno055_udp_sender/xiao_bno055_udp_sender.ino](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\firmware\xiao_bno055_udp_sender\xiao_bno055_udp_sender.ino)

## Data Path

1. The XIAO reads fused orientation data from the BNO055.
2. The XIAO sends packets over Wi-Fi using `UDP`.
3. The `raylib` app listens on port `4210`.
4. In game mode, the app steers a toy car on a generated road-carpet map using quaternion-derived twist around the sensor pitch axis by default.
5. In hardware test mode, the app rotates an on-screen steering wheel based on the selected incoming quaternion-derived axis twist.

Packet format:

```text
qw=0.996,qx=0.012,qy=-0.084,qz=0.018,button1=0,button2=1
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

`REPLACE_WITH_HOST_IP` is now mainly a fallback / reference value. In the current discovery flow, the controller waits for the game server to advertise itself over UDP broadcast before it marks setup complete.

Important note:

- attach the XIAO's external Wi-Fi / BLE antenna before expecting reliable wireless behavior
- the firmware sets a friendlier Wi-Fi hostname: `steering-wheel-poc-esp32c3`
- the serial monitor prints explicit `UDP #... sent to ...` lines for successful sends
- after Wi-Fi connects, the controller keeps the status LED in the breathing setup state until it hears a valid server discovery broadcast on UDP `4211`
- once discovery succeeds, the controller locks onto that sender IP for the rest of the run and stops listening for other servers

## Serial Diagnostics

Open Serial Monitor at `115200` after upload. The firmware prints:

- the expected XIAO pin map for `BNO055`, buttons, and status LED
- initial button states, followed by button state changes only when they happen
- an `I2C` scan before initializing the BNO055; the BNO055 should usually appear at `0x28`
- BNO055 sensor details after successful detection
- visible Wi-Fi networks, connection status changes, local IP, and signal strength
- a compact `Health:` block every `5` seconds with Wi-Fi status, UDP send count, quaternion orientation, calibration, and button states

Set `kSerialDebugEnabled` near the top of the firmware sketch to `false` for live use. That disables Serial output and skips troubleshooting-only scans/reports while keeping the controller, Wi-Fi, UDP packets, buttons, and status LED behavior active.

## Desktop App Behavior

- default mode is the Road Carpet Drive game
- `T` toggles between the game and hardware test dashboard
- `F11` toggles fullscreen
- `A` toggles auto-drive / button-throttle mode in the game
- `P` uses quaternion-derived twist around the sensor pitch axis for steering
- `R` uses quaternion-derived twist around the sensor roll axis for debug comparison
- `Y` uses quaternion-derived twist around the sensor yaw axis for debug comparison
- `SPACE` captures the current sensor orientation as the center, so switching between the three twist axes stays calibrated
- `A` / `D` or left / right arrows provide a keyboard steering fallback when packets are not arriving
- `W` / up arrow and `S` / down arrow provide keyboard acceleration/brake fallback in button-throttle mode
- the app displays the latest quaternion values, derived Euler readout, `button1`, and `button2` values from UDP packets
- the hardware test dashboard shows `button2` as a red lamp on the left and `button1` as a green lamp on the right
- the app shows the host IPv4 address in the window so it is easy to confirm which machine is advertising itself on the LAN
- the app keeps the last received value on screen even if packets go stale
- if the game has not received controller packets for more than `5` seconds, it sends a small UDP broadcast beacon once per second on port `4211` so a booting controller can discover it

## Game Assets

The game uses generated original assets in `controller_poc/assets`:

- `road_carpet_map_2.png`
- `sports_car_top.png`
- `coin.png`
- `carpet_cruise_loop.wav`
- `toy_engine_loop.wav`
- `coin_chime.wav`

`toy_car_top.png` is kept as an alternate car sprite.

Run `python tools/generate_assets.py` and `python tools/generate_sounds.py` from `controller_poc` to regenerate the current art and sound assets.

## Controller GPIO

The current XIAO ESP32-C3 firmware uses:

| Function | XIAO pin | Wiring |
| --- | --- | --- |
| `BNO055 SDA` | `D4` | sensor `SDA` |
| `BNO055 SCL` | `D5` | sensor `SCL` |
| `button1` | `D1` | button to `GND`, uses `INPUT_PULLUP` |
| `button2` | `D2` | button to `GND`, uses `INPUT_PULLUP` |
| status LED | `D10` | GPIO -> resistor -> LED -> `GND` |

Status LED wiring notes:

- A `330 ohm` resistor is a good default for a normal green LED on `3.3V`.
- The `560 ohm` resistor with `green-blue-brown-gold` bands is also safe; it will just be dimmer.
- The resistor can go on either side of the LED and has no direction.
- The LED does have direction: long leg / round side is the anode and goes toward `D10`; short leg / flat side is the cathode and goes to `GND`.

Status LED states:

- slow breathing: setup is still in progress, starting as soon as the sketch boots
- brief off beat, then rapid flash for about `0.5` seconds: setup completed and Wi-Fi connected
- solid: runtime is healthy and data transmission has started
- `500ms` on/off blink: error state

The status LED is driven by a millis-based state machine. Wi-Fi setup, retry waits, and debug Wi-Fi scanning all call the LED updater instead of playing blocking LED animations with long `delay(...)` calls.

## Build Notes

### Windows

The POC expects a `raylib` checkout at `controller_poc/../raylib` by default, similar to your existing workflow. If needed, set `RAYLIB_DIR` before building.

Run logic tests with `windows_test.bat`.

### Linux

The POC expects a system `raylib` package discoverable through `pkg-config`.

## Suggested First Test

1. Build and launch the `raylib` app.
2. Confirm Road Carpet Drive loads by default.
3. Fill in Wi-Fi credentials in `wifi_secrets.h`.
4. Upload the firmware.
5. Wait for the game to begin broadcasting discovery beacons if it has not heard from a controller for `5` seconds.
6. Confirm the controller fast-flashes once it locks the server IP, then switches to live `UDP` steering input.
7. Press `T` to verify the hardware test dashboard.

## Real-World Networking Notes

This was verified working on a network where the Windows PC and XIAO were on different VLANs / network segments.

What mattered in practice:

- Windows needed an inbound firewall allow rule for the app or UDP port `4210`
- the router also needed an allow rule for UDP traffic between the relevant network segments
- local loopback testing from PowerShell to the app was useful to separate app issues from network issues
