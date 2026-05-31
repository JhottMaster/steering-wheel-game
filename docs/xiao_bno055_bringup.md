# XIAO ESP32-C3 + BNO055 Bring-Up

This is the fastest path to validate the riskiest part of the project:

- `Seeed Studio XIAO ESP32-C3`
- `Adafruit BNO055`
- `USB-C` power/data to a computer or Raspberry Pi

## Goal

Confirm all of the following as quickly as possible:

1. The XIAO can be programmed over `USB-C`.
2. The BNO055 is detected reliably over `I2C`.
3. We can print orientation data to `Serial` over USB.

## Wiring

Use `I2C` for the first test.

| XIAO ESP32-C3 | BNO055 |
| --- | --- |
| `3V3` | `VIN` |
| `GND` | `GND` |
| `D4 / SDA` | `SDA` |
| `D5 / SCL` | `SCL` |

Notes:

- On the XIAO ESP32-C3, Seeed maps `D4` to `SDA` and `D5` to `SCL`.
- The Adafruit BNO055 breakout accepts `3.3V` on `VIN`, so powering it from the XIAO `3V3` pin is fine.
- Keep the wires short for the first test. The BNO055 can be fussy on `I2C`.

## Arduino IDE Setup

Use the current `Arduino IDE 2.x`.

1. Install Arduino IDE.
2. Open `File -> Preferences`.
3. Add this to `Additional Boards Manager URLs`:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

4. Open `Tools -> Board -> Boards Manager`.
5. Search for `esp32`.
6. Install the `esp32` package from `Espressif Systems`.
7. Select `Tools -> Board -> ESP32 Arduino -> XIAO_ESP32C3`.
8. Select the correct `Tools -> Port`.

Then install libraries with `Library Manager`:

- `Adafruit BNO055`
- `Adafruit Unified Sensor`

## Smoke Test Sketch

The sketch lives here:

- [xiao_bno055_smoketest.ino](C:\Users\Pablo\Desktop\Projects\3D Printing\Toddler Steering Wheel\steering-wheel-game\firmware\xiao_bno055_smoketest\xiao_bno055_smoketest.ino)

What it does:

- starts USB serial
- waits briefly after reset so repeated reprogramming is less annoying
- starts `I2C` on `D4` and `D5`
- slows the bus to `100 kHz`
- attempts to initialize the BNO055
- prints Euler orientation and calibration values repeatedly

## Test Procedure

1. Wire the boards.
2. Plug the XIAO into your computer with a data-capable `USB-C` cable.
3. Upload the smoke test sketch.
4. Open `Serial Monitor`.
5. Set baud to `115200`.
6. Press reset once if needed.

Success looks like:

- `BNO055 detected`
- repeated lines of heading / roll / pitch values
- calibration values changing as you move the sensor

## Known Risk

Adafruit notes that the BNO055 can have `I2C` compatibility issues with `ESP32`-family boards. The first purpose of this test is to find out whether `XIAO ESP32-C3 + BNO055` behaves well enough for us.

If it is unstable:

- keep wires shorter
- try a different USB cable
- power-cycle both boards
- try adding stronger `I2C` pullups later
- fall back to the `LIS3DH` or a different IMU if necessary

## QoL Notes

- The XIAO ESP32-C3 does not have a normal programmable onboard user LED. The tiny red LED visible on the board is effectively just a power indicator.
- The smoke-test sketch now includes a short quiet startup window after reset so the serial port is easier to reconnect to while iterating.

## Controller Direction

For the next prototype step, the fastest path into a `raylib` C++ game is:

1. keep using `USB-C`
2. send a compact steering value over `USB serial`
3. read that serial stream from the desktop or Raspberry Pi app

This is simpler than trying to start with `BLE HID` or a full USB gamepad emulation path.

## If This Passes

If the smoke test works, the next step is:

1. define the USB access slot in CAD
2. deepen the hub cavity
3. mount the XIAO and BNO055 as the first real electronics prototype

## Sources

- Seeed XIAO ESP32-C3 getting started: https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/
- Adafruit BNO055 guide: https://learn.adafruit.com/adafruit-bno055-absolute-orientation-sensor
