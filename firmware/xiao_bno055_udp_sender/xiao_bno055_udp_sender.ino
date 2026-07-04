#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include <utility/imumaths.h>
#include "wifi_secrets.h"

namespace {
constexpr char kWifiHostname[] = "steering-wheel-poc-esp32c3";
constexpr uint16_t kHostPort = 4210;
constexpr uint16_t kDiscoveryPort = 4211;
constexpr char kDiscoveryMessage[] = "steering-wheel-server port=4210";
constexpr int kSdaPin = D4;
constexpr int kSclPin = D5;
constexpr int kButton1Pin = D1;
constexpr int kButton2Pin = D2;
constexpr int kStatusLedPin = D10;
constexpr int kSdaGpioNumber = 6;
constexpr int kSclGpioNumber = 7;
constexpr int kButton1GpioNumber = 3;
constexpr int kButton2GpioNumber = 4;
constexpr int kStatusLedGpioNumber = 10;
constexpr bool kSerialDebugEnabled = true;
constexpr uint32_t kBaudRate = 115200;
constexpr uint32_t kStartupQuietMs = 4000;
constexpr uint32_t kI2cClockHz = 100000;
constexpr uint32_t kPacketIntervalMs = 33;
constexpr uint32_t kHeartbeatIntervalMs = 200;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiScanTimeoutMs = 8000;
constexpr uint32_t kWifiRetryDelayMs = 3000;
constexpr uint32_t kStatusLedBreathPeriodMs = 2200;
constexpr uint32_t kStatusLedBreathStepMs = 25;
constexpr uint32_t kStatusLedErrorFlashMs = 500;
constexpr uint32_t kStatusLedSetupCompleteFlashMs = 500;
constexpr uint32_t kStatusLedSetupCompleteToggleMs = 50;
constexpr uint32_t kStatusLedSetupCompleteBlankMs = 100;
constexpr uint32_t kHealthReportIntervalMs = 5000;
constexpr uint32_t kDiscoveryHeartbeatIntervalMs = 3000;
constexpr float kMinQuaternionDelta = 0.015f;
uint32_t wifiAttemptCount = 0;
uint32_t udpSendCount = 0;

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
WiFiUDP udp;
WiFiUDP discoveryUdp;
uint32_t lastPacketMs = 0;
bool hasLastSentFrame = false;
float lastSentQuatW = 1.0f;
float lastSentQuatX = 0.0f;
float lastSentQuatY = 0.0f;
float lastSentQuatZ = 0.0f;
bool lastSentButton1Pressed = false;
bool lastSentButton2Pressed = false;
bool hasLastReportedButtons = false;
bool lastReportedButton1Pressed = false;
bool lastReportedButton2Pressed = false;
uint32_t lastHealthReportMs = 0;
uint32_t lastSuccessfulSendMs = 0;
IPAddress hostIp;
bool hasHostIp = false;

enum class StatusLedMode {
  kSetupBreathing,
  kSetupCompleteFlash,
  kRuntimeSolid,
  kErrorBlink,
};

StatusLedMode statusLedMode = StatusLedMode::kSetupBreathing;
uint32_t statusLedModeStartMs = 0;
bool runtimeSolidPending = false;

class DebugSerialProxy {
 public:
  void begin(uint32_t baudRate) const {
    if (kSerialDebugEnabled) {
      ::Serial.begin(baudRate);
    }
  }

  explicit operator bool() const {
    return !kSerialDebugEnabled || static_cast<bool>(::Serial);
  }

  template <typename... Args>
  void print(Args... args) const {
    if (kSerialDebugEnabled) {
      ::Serial.print(args...);
    }
  }

  template <typename... Args>
  void println(Args... args) const {
    if (kSerialDebugEnabled) {
      ::Serial.println(args...);
    }
  }
};

DebugSerialProxy DebugSerial;
#define Serial DebugSerial

const char* wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "idle";
    case WL_NO_SSID_AVAIL:
      return "ssid not available";
    case WL_SCAN_COMPLETED:
      return "scan completed";
    case WL_CONNECTED:
      return "connected";
    case WL_CONNECT_FAILED:
      return "connect failed";
    case WL_CONNECTION_LOST:
      return "connection lost";
    case WL_DISCONNECTED:
      return "disconnected";
    default:
      return "unknown";
  }
}

bool quaternionComponentChangedEnough(float previousValue, float currentValue) {
  return fabsf(currentValue - previousValue) >= kMinQuaternionDelta;
}

bool readButtonPressed(int pin) {
  return digitalRead(pin) == LOW;
}

void setStatusLed(bool enabled) {
  analogWrite(kStatusLedPin, enabled ? 255 : 0);
}

void setStatusLedBrightness(uint8_t brightness) {
  analogWrite(kStatusLedPin, brightness);
}

uint8_t breathingBrightness(uint32_t now) {
  const uint32_t halfPeriodMs = kStatusLedBreathPeriodMs / 2;
  const uint32_t phaseMs = now % kStatusLedBreathPeriodMs;
  const uint32_t rampMs = phaseMs < halfPeriodMs ? phaseMs : kStatusLedBreathPeriodMs - phaseMs;
  return static_cast<uint8_t>(map(rampMs, 0, halfPeriodMs, 8, 255));
}

void updateSetupStatusLed(uint32_t now) {
  setStatusLedBrightness(breathingBrightness(now));
}

void setStatusLedMode(StatusLedMode mode, uint32_t now) {
  statusLedMode = mode;
  statusLedModeStartMs = now;
  if (mode == StatusLedMode::kRuntimeSolid) {
    runtimeSolidPending = false;
  }
}

void requestSetupCompleteFlash(uint32_t now) {
  runtimeSolidPending = false;
  setStatusLedMode(StatusLedMode::kSetupCompleteFlash, now);
}

void requestRuntimeSolid(uint32_t now) {
  if (statusLedMode == StatusLedMode::kSetupCompleteFlash) {
    runtimeSolidPending = true;
    return;
  }

  setStatusLedMode(StatusLedMode::kRuntimeSolid, now);
}

void requestErrorBlink(uint32_t now) {
  setStatusLedMode(StatusLedMode::kErrorBlink, now);
}

void updateStatusLed(uint32_t now) {
  switch (statusLedMode) {
    case StatusLedMode::kSetupBreathing:
      updateSetupStatusLed(now);
      break;

    case StatusLedMode::kSetupCompleteFlash: {
      const uint32_t elapsedMs = now - statusLedModeStartMs;
      if (elapsedMs < kStatusLedSetupCompleteBlankMs) {
        setStatusLed(false);
      } else if (elapsedMs < kStatusLedSetupCompleteBlankMs + kStatusLedSetupCompleteFlashMs) {
        const uint32_t flashElapsedMs = elapsedMs - kStatusLedSetupCompleteBlankMs;
        setStatusLed((flashElapsedMs / kStatusLedSetupCompleteToggleMs) % 2 == 0);
      } else if (runtimeSolidPending) {
        setStatusLedMode(StatusLedMode::kRuntimeSolid, now);
        setStatusLed(true);
      } else {
        setStatusLed(false);
      }
      break;
    }

    case StatusLedMode::kRuntimeSolid:
      setStatusLed(true);
      break;

    case StatusLedMode::kErrorBlink:
      setStatusLed(((now - statusLedModeStartMs) / kStatusLedErrorFlashMs) % 2 == 0);
      break;
  }
}

void waitWithStatusLed(uint32_t durationMs) {
  const uint32_t startMs = millis();
  while (millis() - startMs < durationMs) {
    updateStatusLed(millis());
    delay(5);
  }
}

void showErrorStatusLedFor(uint32_t durationMs) {
  requestErrorBlink(millis());
  waitWithStatusLed(durationMs);
  setStatusLedMode(StatusLedMode::kSetupBreathing, millis());
}

void blinkStatusLedForever() {
  requestErrorBlink(millis());
  while (true) {
    updateStatusLed(millis());
    delay(5);
  }
}

void printPressedState(bool pressed) {
  Serial.print(pressed ? "pressed" : "released");
}

void printControllerPinout() {
  Serial.println("Controller pin map:");
  Serial.print("  BNO055 SDA: D4 / GPIO ");
  Serial.println(kSdaGpioNumber);
  Serial.print("  BNO055 SCL: D5 / GPIO ");
  Serial.println(kSclGpioNumber);
  Serial.print("  button1: D1 / GPIO ");
  Serial.print(kButton1GpioNumber);
  Serial.println(" -> button -> GND, INPUT_PULLUP");
  Serial.print("  button2: D2 / GPIO ");
  Serial.print(kButton2GpioNumber);
  Serial.println(" -> button -> GND, INPUT_PULLUP");
  Serial.print("  status LED: D10 / GPIO ");
  Serial.print(kStatusLedGpioNumber);
  Serial.println(" -> resistor -> LED anode, LED cathode -> GND");
}

void printButtonSnapshot(const char* prefix, bool button1Pressed, bool button2Pressed) {
  Serial.print(prefix);
  Serial.print("button1=");
  printPressedState(button1Pressed);
  Serial.print(", button2=");
  printPressedState(button2Pressed);
  Serial.println();
}

void reportButtonChanges(bool button1Pressed, bool button2Pressed) {
  if (!kSerialDebugEnabled) {
    return;
  }

  if (hasLastReportedButtons && lastReportedButton1Pressed == button1Pressed &&
      lastReportedButton2Pressed == button2Pressed) {
    return;
  }

  printButtonSnapshot("Button state changed: ", button1Pressed, button2Pressed);
  hasLastReportedButtons = true;
  lastReportedButton1Pressed = button1Pressed;
  lastReportedButton2Pressed = button2Pressed;
}

void printI2cScan() {
  if (!kSerialDebugEnabled) {
    return;
  }

  Serial.println("Scanning I2C bus...");
  uint8_t deviceCount = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("  found device at 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      ++deviceCount;
    } else if (error == 4) {
      Serial.print("  unknown I2C error at 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }

  if (deviceCount == 0) {
    Serial.println("  no I2C devices found; check 3V3, GND, SDA, and SCL wiring");
  }
}

void printBnoDetails() {
  if (!kSerialDebugEnabled) {
    return;
  }

  sensor_t sensor;
  bno.getSensor(&sensor);
  Serial.println("BNO055 details:");
  Serial.print("  sensor: ");
  Serial.println(sensor.name);
  Serial.print("  driver version: ");
  Serial.println(sensor.version);
  Serial.print("  sensor id: ");
  Serial.println(sensor.sensor_id);
  Serial.print("  max value: ");
  Serial.println(sensor.max_value);
  Serial.print("  min value: ");
  Serial.println(sensor.min_value);
  Serial.print("  resolution: ");
  Serial.println(sensor.resolution);
}

void printRuntimeHealth(uint32_t now, const imu::Quaternion& quaternion,
                        bool button1Pressed, bool button2Pressed) {
  if (!kSerialDebugEnabled) {
    return;
  }

  if (lastHealthReportMs != 0 && now - lastHealthReportMs < kHealthReportIntervalMs) {
    return;
  }
  lastHealthReportMs = now;

  uint8_t systemCalibration = 0;
  uint8_t gyroCalibration = 0;
  uint8_t accelCalibration = 0;
  uint8_t magCalibration = 0;
  bno.getCalibration(&systemCalibration, &gyroCalibration, &accelCalibration, &magCalibration);

  Serial.println("Health:");
  Serial.print("  uptime_ms=");
  Serial.println(now);
  Serial.print("  wifi=");
  Serial.print(wifiStatusToString(WiFi.status()));
  Serial.print(" rssi=");
  Serial.print(WiFi.RSSI());
  Serial.print(" local_ip=");
  Serial.println(WiFi.localIP());
  Serial.print("  udp_sent=");
  Serial.println(udpSendCount);
  Serial.print("  host_ip=");
  if (hasHostIp) {
    Serial.println(hostIp);
  } else {
    Serial.println("not discovered");
  }
  Serial.print("  quaternion w=");
  Serial.print(quaternion.w(), 4);
  Serial.print(" x=");
  Serial.print(quaternion.x(), 4);
  Serial.print(" y=");
  Serial.print(quaternion.y(), 4);
  Serial.print(" z=");
  Serial.println(quaternion.z(), 4);
  Serial.print("  calibration sys=");
  Serial.print(systemCalibration);
  Serial.print(" gyro=");
  Serial.print(gyroCalibration);
  Serial.print(" accel=");
  Serial.print(accelCalibration);
  Serial.print(" mag=");
  Serial.println(magCalibration);
  printButtonSnapshot("  buttons ", button1Pressed, button2Pressed);
}

void printVisibleNetworks() {
  if (!kSerialDebugEnabled) {
    return;
  }

  Serial.println("Scanning for visible Wi-Fi networks...");
  WiFi.scanNetworks(true);
  const uint32_t scanStartMs = millis();
  int networkCount = WiFi.scanComplete();
  while (networkCount == -1 && millis() - scanStartMs < kWifiScanTimeoutMs) {
    updateStatusLed(millis());
    delay(5);
    networkCount = WiFi.scanComplete();
  }

  if (networkCount == -1) {
    Serial.println("Wi-Fi scan timed out.");
    WiFi.scanDelete();
    return;
  }

  if (networkCount <= 0) {
    Serial.println("No Wi-Fi networks found.");
    WiFi.scanDelete();
    return;
  }

  Serial.print("Found ");
  Serial.print(networkCount);
  Serial.println(" network(s):");
  for (int i = 0; i < networkCount; ++i) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" RSSI=");
    Serial.print(WiFi.RSSI(i));
    Serial.print(" dBm");
    Serial.print(" channel=");
    Serial.print(WiFi.channel(i));
    Serial.print(" ");
    Serial.println(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured");
  }
  WiFi.scanDelete();
}

void printWifiStatusLine(const char* prefix, wl_status_t status) {
  Serial.print(prefix);
  Serial.print(wifiStatusToString(status));
  Serial.print(" (");
  Serial.print(static_cast<int>(status));
  Serial.println(")");
}

void resetWifiRadio() {
  Serial.println("Resetting Wi-Fi radio...");
  WiFi.disconnect(true, true);
  waitWithStatusLed(250);
  WiFi.mode(WIFI_OFF);
  waitWithStatusLed(250);
  WiFi.mode(WIFI_STA);
  waitWithStatusLed(250);
}

bool connectToWifi() {
  ++wifiAttemptCount;
  resetWifiRadio();

  Serial.println();
  Serial.print("Wi-Fi attempt #");
  Serial.println(wifiAttemptCount);
  Serial.println("Wi-Fi setup:");
  Serial.print("  Hostname: ");
  Serial.println(kWifiHostname);
  Serial.print("  SSID: ");
  Serial.println(kWifiSecretsSsid);
  Serial.print("  Host IP: ");
  Serial.println(kWifiSecretsHostIp);
  Serial.print("  Discovery port: ");
  Serial.println(kDiscoveryPort);
  Serial.print("  UDP port: ");
  Serial.println(kHostPort);

  printVisibleNetworks();

  Serial.print("Connecting to Wi-Fi");
  WiFi.setHostname(kWifiHostname);
  WiFi.begin(kWifiSecretsSsid, kWifiSecretsPassword);
  const uint32_t connectStartMs = millis();
  uint32_t lastProgressDotMs = 0;
  wl_status_t lastStatus = WiFi.status();
  printWifiStatusLine("Initial Wi-Fi status: ", lastStatus);

  while ((millis() - connectStartMs) < kWifiConnectTimeoutMs) {
    const uint32_t now = millis();
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
      Serial.println();
      Serial.print("Wi-Fi connected. Local IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal strength: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      return true;
    }

    if (status != lastStatus) {
      Serial.println();
      printWifiStatusLine("Wi-Fi status changed: ", status);
      lastStatus = status;
      Serial.print("Connecting to Wi-Fi");
    }

    if (now - lastProgressDotMs >= 500) {
      Serial.print(".");
      lastProgressDotMs = now;
    }
    updateStatusLed(now);
    delay(5);
  }

  Serial.println();
  Serial.println("Wi-Fi connection timed out.");
  printWifiStatusLine("Final Wi-Fi status: ", WiFi.status());
  printVisibleNetworks();
  Serial.println("If this keeps failing, test with a simple 2.4 GHz phone hotspot to separate router issues from board issues.");
  showErrorStatusLedFor(4000);
  return false;
}

bool discoverHost() {
  if (hasHostIp) {
    return true;
  }

  Serial.println("Listening for controller game server broadcast...");
  Serial.print("  Discovery port: ");
  Serial.println(kDiscoveryPort);
  Serial.print("  Expected message: ");
  Serial.println(kDiscoveryMessage);

  if (!discoveryUdp.begin(kDiscoveryPort)) {
    Serial.println("Failed to open UDP discovery listener.");
    showErrorStatusLedFor(3000);
    return false;
  }

  uint32_t lastDiscoveryHeartbeatMs = 0;
  while (!hasHostIp) {
    const uint32_t now = millis();
    updateStatusLed(now);

    if (lastDiscoveryHeartbeatMs == 0 ||
        now - lastDiscoveryHeartbeatMs >= kDiscoveryHeartbeatIntervalMs) {
      Serial.print("Still listening for server discovery on UDP ");
      Serial.print(kDiscoveryPort);
      Serial.print(" from local IP ");
      Serial.println(WiFi.localIP());
      lastDiscoveryHeartbeatMs = now;
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi disconnected while waiting for server discovery.");
      discoveryUdp.stop();
      return false;
    }

    const int packetSize = discoveryUdp.parsePacket();
    if (packetSize <= 0) {
      delay(5);
      continue;
    }

    char packet[96];
    const int bytesRead =
        discoveryUdp.read(packet, static_cast<int>(sizeof(packet) - 1));
    if (bytesRead <= 0) {
      delay(5);
      continue;
    }
    packet[bytesRead] = '\0';

    Serial.print("Discovery packet from ");
    Serial.print(discoveryUdp.remoteIP());
    Serial.print(":");
    Serial.print(discoveryUdp.remotePort());
    Serial.print(" -> ");
    Serial.println(packet);

    if (strcmp(packet, kDiscoveryMessage) != 0) {
      Serial.println("Ignoring unexpected discovery payload.");
      delay(5);
      continue;
    }

    hostIp = discoveryUdp.remoteIP();
    hasHostIp = true;
  }

  discoveryUdp.stop();
  Serial.print("Locked server IP: ");
  Serial.println(hostIp);
  requestSetupCompleteFlash(millis());
  Serial.println("Status LED: setup complete flash.");
  return true;
}
}  // namespace

void setup() {
  pinMode(kButton1Pin, INPUT_PULLUP);
  pinMode(kButton2Pin, INPUT_PULLUP);
  pinMode(kStatusLedPin, OUTPUT);
  setStatusLed(false);

  Serial.begin(kBaudRate);
  const uint32_t bootStartMs = millis();
  while (!Serial && (millis() - bootStartMs) < 1500) {
    updateStatusLed(millis());
    delay(5);
  }

  Serial.println();
  Serial.println("XIAO ESP32-C3 + BNO055 UDP sender");
  printControllerPinout();
  printButtonSnapshot("Initial buttons: ", readButtonPressed(kButton1Pin), readButtonPressed(kButton2Pin));
  Serial.print("Quiet startup window: ");
  Serial.print(kStartupQuietMs);
  Serial.println(" ms");
  waitWithStatusLed(kStartupQuietMs);

  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cClockHz);
  Serial.print("I2C started at ");
  Serial.print(kI2cClockHz);
  Serial.println(" Hz.");
  printI2cScan();

  Serial.println("Initializing BNO055...");
  updateStatusLed(millis());
  if (!bno.begin()) {
    Serial.println("BNO055 not detected.");
    blinkStatusLedForever();
  }

  waitWithStatusLed(1000);
  bno.setExtCrystalUse(true);
  Serial.println("BNO055 detected.");
  printBnoDetails();

  while (!connectToWifi()) {
    Serial.print("Retrying Wi-Fi connection in ");
    Serial.print(kWifiRetryDelayMs);
    Serial.println(" ms...");
    waitWithStatusLed(kWifiRetryDelayMs);
  }

  while (!discoverHost()) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Still waiting for a valid server discovery packet...");
      waitWithStatusLed(250);
      continue;
    }

    while (!connectToWifi()) {
      Serial.print("Retrying Wi-Fi connection in ");
      Serial.print(kWifiRetryDelayMs);
      Serial.println(" ms...");
      waitWithStatusLed(kWifiRetryDelayMs);
    }
  }
}

void loop() {
  const uint32_t now = millis();
  updateStatusLed(now);

  if (now - lastPacketMs < kPacketIntervalMs) {
    return;
  }
  lastPacketMs = now;

  const imu::Quaternion quaternion = bno.getQuat();
  const float quatW = quaternion.w();
  const float quatX = quaternion.x();
  const float quatY = quaternion.y();
  const float quatZ = quaternion.z();
  const bool button1Pressed = readButtonPressed(kButton1Pin);
  const bool button2Pressed = readButtonPressed(kButton2Pin);
  reportButtonChanges(button1Pressed, button2Pressed);
  printRuntimeHealth(now, quaternion, button1Pressed, button2Pressed);

  const bool shouldSend =
      !hasLastSentFrame || quaternionComponentChangedEnough(lastSentQuatW, quatW) ||
      quaternionComponentChangedEnough(lastSentQuatX, quatX) ||
      quaternionComponentChangedEnough(lastSentQuatY, quatY) ||
      quaternionComponentChangedEnough(lastSentQuatZ, quatZ) ||
      lastSentButton1Pressed != button1Pressed || lastSentButton2Pressed != button2Pressed ||
      lastSuccessfulSendMs == 0 || now - lastSuccessfulSendMs >= kHeartbeatIntervalMs;
  if (!shouldSend) {
    return;
  }

  char packet[128];
  snprintf(packet, sizeof(packet), "qw=%.5f,qx=%.5f,qy=%.5f,qz=%.5f,button1=%d,button2=%d",
           quatW, quatX, quatY, quatZ, button1Pressed ? 1 : 0, button2Pressed ? 1 : 0);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected while sending. Attempting reconnect...");
    while (!connectToWifi()) {
      Serial.print("Retrying Wi-Fi connection in ");
      Serial.print(kWifiRetryDelayMs);
      Serial.println(" ms...");
      waitWithStatusLed(kWifiRetryDelayMs);
    }
  }

  if (!hasHostIp) {
    Serial.println("No discovered host IP is available.");
    showErrorStatusLedFor(3000);
    return;
  }

  if (!udp.beginPacket(hostIp, kHostPort)) {
    Serial.println("Failed to begin UDP packet.");
    showErrorStatusLedFor(3000);
    return;
  }

  udp.write(reinterpret_cast<const uint8_t*>(packet), strlen(packet));
  if (!udp.endPacket()) {
    Serial.println("Failed to send UDP packet.");
    showErrorStatusLedFor(3000);
    return;
  }

  ++udpSendCount;
  lastSuccessfulSendMs = now;
  hasLastSentFrame = true;
  lastSentQuatW = quatW;
  lastSentQuatX = quatX;
  lastSentQuatY = quatY;
  lastSentQuatZ = quatZ;
  lastSentButton1Pressed = button1Pressed;
  lastSentButton2Pressed = button2Pressed;
  requestRuntimeSolid(millis());
  Serial.print("UDP #");
  Serial.print(udpSendCount);
  Serial.print(" sent to ");
  Serial.print(hostIp);
  Serial.print(":");
  Serial.print(kHostPort);
  Serial.print(" -> ");
  Serial.println(packet);
}
