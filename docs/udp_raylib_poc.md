# UDP Raylib POC

This proof of concept connects the current hardware bring-up to a very small cross-platform game shell:

- `XIAO ESP32-C3`
- `BNO055`
- `Wi-Fi UDP`
- `raylib` desktop app on Windows or Linux

## Goal

Show a blank game placeholder by default, with a `T`-toggle hardware test dashboard that visualizes BNO055 orientation and button input.

## Repo Layout

- [controller_poc/src/main.cpp](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\src\main.cpp)
- [controller_poc/Makefile](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\Makefile)
- [controller_poc/windows_build.bat](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\controller_poc\windows_build.bat)
- [firmware/xiao_bno055_udp_sender/xiao_bno055_udp_sender.ino](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\firmware\xiao_bno055_udp_sender\xiao_bno055_udp_sender.ino)

## Data Path

1. The XIAO reads fused orientation data from the BNO055.
2. The XIAO sends packets over Wi-Fi using `UDP`.
3. The `raylib` app listens on port `4210`.
4. In hardware test mode, the app rotates an on-screen steering wheel based on the selected incoming orientation axis.

Packet format:

```text
roll=12.4,pitch=-3.1,heading=182.0,button1=0,button2=1
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

## Serial Diagnostics

Open Serial Monitor at `115200` after upload. The firmware prints:

- the expected XIAO pin map for `BNO055`, buttons, and status LED
- initial button states, followed by button state changes only when they happen
- an `I2C` scan before initializing the BNO055; the BNO055 should usually appear at `0x28`
- BNO055 sensor details after successful detection
- visible Wi-Fi networks, connection status changes, local IP, and signal strength
- a compact `Health:` block every `5` seconds with Wi-Fi status, UDP send count, orientation, calibration, and button states

Set `kSerialDebugEnabled` near the top of the firmware sketch to `false` for live use. That disables Serial output and skips troubleshooting-only scans/reports while keeping the controller, Wi-Fi, UDP packets, buttons, and status LED behavior active.

## Desktop App Behavior

- `T` toggles between the blank game placeholder and hardware test dashboard
- `P` uses `pitch` for steering
- `R` uses `roll` for debug comparison
- `Y` uses `yaw` / `heading` for debug comparison
- `SPACE` captures the current sensor orientation as the center, so switching between `roll`, `pitch`, and `yaw` stays calibrated
- `A` / `D` or left / right arrows provide a keyboard fallback when packets are not arriving
- the app displays the latest `roll`, `pitch`, `heading`, `button1`, and `button2` values from UDP packets
- the hardware test dashboard shows `button2` as a red lamp on the left and `button1` as a green lamp on the right
- the app shows the host IPv4 address in the window so it is easy to copy into `wifi_secrets.h`
- the app keeps the last received value on screen even if packets go stale

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
