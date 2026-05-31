#include <Wire.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include <utility/imumaths.h>

namespace {
constexpr int kSdaPin = D4;
constexpr int kSclPin = D5;
constexpr uint32_t kBaudRate = 115200;
constexpr uint32_t kI2cClockHz = 100000;
constexpr uint32_t kPrintIntervalMs = 200;

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
uint32_t lastPrintMs = 0;

void printCalibration() {
  uint8_t system = 0;
  uint8_t gyro = 0;
  uint8_t accel = 0;
  uint8_t mag = 0;
  bno.getCalibration(&system, &gyro, &accel, &mag);

  Serial.print(" calib sys=");
  Serial.print(system);
  Serial.print(" gyro=");
  Serial.print(gyro);
  Serial.print(" accel=");
  Serial.print(accel);
  Serial.print(" mag=");
  Serial.print(mag);
}
}  // namespace

void setup() {
  Serial.begin(kBaudRate);
  delay(1500);

  Serial.println();
  Serial.println("XIAO ESP32-C3 + BNO055 smoke test");
  Serial.println("Starting I2C...");

  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cClockHz);

  Serial.println("Initializing BNO055...");
  if (!bno.begin()) {
    Serial.println("BNO055 not detected.");
    Serial.println("Check wiring, board power, and I2C pins.");
    while (true) {
      delay(1000);
    }
  }

  delay(1000);
  bno.setExtCrystalUse(true);

  Serial.println("BNO055 detected.");
  Serial.println("Move the sensor and watch heading/roll/pitch.");
}

void loop() {
  const uint32_t now = millis();
  if (now - lastPrintMs < kPrintIntervalMs) {
    return;
  }
  lastPrintMs = now;

  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);

  Serial.print("heading=");
  Serial.print(euler.x(), 1);
  Serial.print(" roll=");
  Serial.print(euler.z(), 1);
  Serial.print(" pitch=");
  Serial.print(euler.y(), 1);
  printCalibration();
  Serial.println();
}
