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
constexpr uint32_t kBaudRate = 115200;
constexpr uint32_t kStartupQuietMs = 4000;
constexpr uint32_t kI2cClockHz = 100000;
constexpr uint32_t kPacketIntervalMs = 33;
constexpr uint32_t kWifiConnectTimeoutMs = 12000;
constexpr uint32_t kWifiRetryDelayMs = 3000;
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
  wl_status_t lastStatus = WiFi.status();
  printWifiStatusLine("Initial Wi-Fi status: ", lastStatus);

  while ((millis() - connectStartMs) < kWifiConnectTimeoutMs) {
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

    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Wi-Fi connection timed out.");
  printWifiStatusLine("Final Wi-Fi status: ", WiFi.status());
  printVisibleNetworks();
  Serial.println("If this keeps failing, test with a simple 2.4 GHz phone hotspot to separate router issues from board issues.");
  return false;
}
}  // namespace

void setup() {
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
  delay(kStartupQuietMs);

  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cClockHz);

  Serial.println("Initializing BNO055...");
  if (!bno.begin()) {
    Serial.println("BNO055 not detected.");
    while (true) {
      delay(1000);
    }
  }

  delay(1000);
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

  const bool shouldSend =
      !hasLastSentFrame || angleChangedEnough(lastSentHeading, heading) ||
      angleChangedEnough(lastSentRoll, roll) || angleChangedEnough(lastSentPitch, pitch);
  if (!shouldSend) {
    return;
  }

  char packet[96];
  snprintf(packet, sizeof(packet), "roll=%.2f,pitch=%.2f,heading=%.2f", roll, pitch, heading);

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
    return;
  }

  udp.write(reinterpret_cast<const uint8_t*>(packet), strlen(packet));
  if (!udp.endPacket()) {
    Serial.println("Failed to send UDP packet.");
    return;
  }

  ++udpSendCount;
  hasLastSentFrame = true;
  lastSentHeading = heading;
  lastSentRoll = roll;
  lastSentPitch = pitch;
  Serial.print("UDP #");
  Serial.print(udpSendCount);
  Serial.print(" sent to ");
  Serial.print(kWifiSecretsHostIp);
  Serial.print(":");
  Serial.print(kHostPort);
  Serial.print(" -> ");
  Serial.println(packet);
}
