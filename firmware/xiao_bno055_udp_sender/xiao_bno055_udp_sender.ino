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
constexpr int kSdaPin = D4;
constexpr int kSclPin = D5;
constexpr int kButton1Pin = D1;
constexpr int kButton2Pin = D2;
constexpr int kStatusLedPin = D10;
constexpr uint32_t kBaudRate = 115200;
constexpr uint32_t kStartupQuietMs = 4000;
constexpr uint32_t kI2cClockHz = 100000;
constexpr uint32_t kPacketIntervalMs = 33;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryDelayMs = 3000;
constexpr uint32_t kStatusLedBreathPeriodMs = 2200;
constexpr uint32_t kStatusLedBreathStepMs = 25;
constexpr uint32_t kStatusLedFastFlashMs = 120;
constexpr float kMinAngleDeltaDeg = 0.5f;
uint32_t wifiAttemptCount = 0;
uint32_t udpSendCount = 0;

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
WiFiUDP udp;
uint32_t lastPacketMs = 0;
bool hasLastSentFrame = false;
float lastSentHeading = 0.0f;
float lastSentRoll = 0.0f;
float lastSentPitch = 0.0f;
bool lastSentButton1Pressed = false;
bool lastSentButton2Pressed = false;

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

bool angleChangedEnough(float previousValue, float currentValue) {
  return fabsf(currentValue - previousValue) >= kMinAngleDeltaDeg;
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

void breatheStatusLedFor(uint32_t durationMs) {
  const uint32_t startMs = millis();
  uint32_t lastBreathStepMs = 0;
  while (millis() - startMs < durationMs) {
    const uint32_t now = millis();
    if (now - lastBreathStepMs >= kStatusLedBreathStepMs) {
      updateSetupStatusLed(now);
      lastBreathStepMs = now;
    }
    delay(5);
  }
}

void fastFlashStatusLed(uint8_t flashCount) {
  for (uint8_t i = 0; i < flashCount; ++i) {
    setStatusLed(true);
    delay(kStatusLedFastFlashMs);
    setStatusLed(false);
    delay(kStatusLedFastFlashMs);
  }
}

void blinkStatusLedForever(uint32_t intervalMs) {
  while (true) {
    setStatusLed(true);
    delay(intervalMs);
    setStatusLed(false);
    delay(intervalMs);
  }
}

void printVisibleNetworks() {
  Serial.println("Scanning for visible Wi-Fi networks...");
  const int networkCount = WiFi.scanNetworks();
  if (networkCount <= 0) {
    Serial.println("No Wi-Fi networks found.");
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
  delay(250);
  WiFi.mode(WIFI_OFF);
  delay(250);
  WiFi.mode(WIFI_STA);
  delay(250);
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
  Serial.print("  UDP port: ");
  Serial.println(kHostPort);

  printVisibleNetworks();

  Serial.print("Connecting to Wi-Fi");
  WiFi.setHostname(kWifiHostname);
  WiFi.begin(kWifiSecretsSsid, kWifiSecretsPassword);
  const uint32_t connectStartMs = millis();
  uint32_t lastBreathStepMs = 0;
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
    if (now - lastBreathStepMs >= kStatusLedBreathStepMs) {
      updateSetupStatusLed(now);
      lastBreathStepMs = now;
    }
    delay(5);
  }

  Serial.println();
  Serial.println("Wi-Fi connection timed out.");
  printWifiStatusLine("Final Wi-Fi status: ", WiFi.status());
  printVisibleNetworks();
  Serial.println("If this keeps failing, test with a simple 2.4 GHz phone hotspot to separate router issues from board issues.");
  fastFlashStatusLed(8);
  return false;
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
    delay(10);
  }

  Serial.println();
  Serial.println("XIAO ESP32-C3 + BNO055 UDP sender");
  Serial.print("Quiet startup window: ");
  Serial.print(kStartupQuietMs);
  Serial.println(" ms");
  breatheStatusLedFor(kStartupQuietMs);

  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cClockHz);

  Serial.println("Initializing BNO055...");
  updateSetupStatusLed(millis());
  if (!bno.begin()) {
    Serial.println("BNO055 not detected.");
    blinkStatusLedForever(kStatusLedFastFlashMs);
  }

  breatheStatusLedFor(1000);
  bno.setExtCrystalUse(true);
  Serial.println("BNO055 detected.");

  while (!connectToWifi()) {
    Serial.print("Retrying Wi-Fi connection in ");
    Serial.print(kWifiRetryDelayMs);
    Serial.println(" ms...");
    delay(kWifiRetryDelayMs);
  }
}

void loop() {
  const uint32_t now = millis();
  if (now - lastPacketMs < kPacketIntervalMs) {
    return;
  }
  lastPacketMs = now;

  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  const float heading = euler.x();
  const float roll = euler.z();
  const float pitch = euler.y();
  const bool button1Pressed = readButtonPressed(kButton1Pin);
  const bool button2Pressed = readButtonPressed(kButton2Pin);

  const bool shouldSend =
      !hasLastSentFrame || angleChangedEnough(lastSentHeading, heading) ||
      angleChangedEnough(lastSentRoll, roll) || angleChangedEnough(lastSentPitch, pitch) ||
      lastSentButton1Pressed != button1Pressed || lastSentButton2Pressed != button2Pressed;
  if (!shouldSend) {
    return;
  }

  char packet[128];
  snprintf(packet, sizeof(packet), "roll=%.2f,pitch=%.2f,heading=%.2f,button1=%d,button2=%d",
           roll, pitch, heading, button1Pressed ? 1 : 0, button2Pressed ? 1 : 0);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected while sending. Attempting reconnect...");
    while (!connectToWifi()) {
      Serial.print("Retrying Wi-Fi connection in ");
      Serial.print(kWifiRetryDelayMs);
      Serial.println(" ms...");
      delay(kWifiRetryDelayMs);
    }
  }

  if (!udp.beginPacket(kWifiSecretsHostIp, kHostPort)) {
    Serial.println("Failed to begin UDP packet.");
    fastFlashStatusLed(5);
    return;
  }

  udp.write(reinterpret_cast<const uint8_t*>(packet), strlen(packet));
  if (!udp.endPacket()) {
    Serial.println("Failed to send UDP packet.");
    fastFlashStatusLed(5);
    return;
  }

  ++udpSendCount;
  hasLastSentFrame = true;
  lastSentHeading = heading;
  lastSentRoll = roll;
  lastSentPitch = pitch;
  lastSentButton1Pressed = button1Pressed;
  lastSentButton2Pressed = button2Pressed;
  setStatusLed(true);
  Serial.print("UDP #");
  Serial.print(udpSendCount);
  Serial.print(" sent to ");
  Serial.print(kWifiSecretsHostIp);
  Serial.print(":");
  Serial.print(kHostPort);
  Serial.print(" -> ");
  Serial.println(packet);
}
