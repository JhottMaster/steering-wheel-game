#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

#include "raylib.h"

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kUdpPort = 4210;
constexpr float kVisibleAngleRangeDeg = 45.0f;
constexpr float kKeyboardStepPerSecond = 35.0f;
constexpr float kPacketTimeoutSeconds = 2.0f;
constexpr float kDegToRad = 0.017453292519943295769f;
constexpr float kSteeringDirection = -1.0f;

enum class DisplayAxis {
  kRoll,
  kPitch,
};

struct SensorFrame {
  float roll = 0.0f;
  float pitch = 0.0f;
  float heading = 0.0f;
};

struct UdpReceiver {
  SocketHandle socket = kInvalidSocket;
  bool winsockStarted = false;

  bool Open(int port) {
#if defined(_WIN32)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
      return false;
    }
    winsockStarted = true;
#endif

    socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == kInvalidSocket) {
      Close();
      return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
      Close();
      return false;
    }

#if defined(_WIN32)
    u_long nonBlocking = 1;
    if (ioctlsocket(socket, FIONBIO, &nonBlocking) != 0) {
      Close();
      return false;
    }
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0 || fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
      Close();
      return false;
    }
#endif

    return true;
  }

  void Close() {
    if (socket != kInvalidSocket) {
#if defined(_WIN32)
      closesocket(socket);
#else
      close(socket);
#endif
      socket = kInvalidSocket;
    }

#if defined(_WIN32)
    if (winsockStarted) {
      WSACleanup();
      winsockStarted = false;
    }
#endif
  }

  ~UdpReceiver() { Close(); }
};

bool ParsePacket(const char* packet, SensorFrame* frame) {
  float roll = 0.0f;
  float pitch = 0.0f;
  float heading = 0.0f;
  if (std::sscanf(packet, "roll=%f,pitch=%f,heading=%f", &roll, &pitch, &heading) != 3) {
    return false;
  }

  frame->roll = roll;
  frame->pitch = pitch;
  frame->heading = heading;
  return true;
}

bool PollLatestSensorFrame(UdpReceiver* receiver, SensorFrame* frame) {
  bool receivedFrame = false;
  std::array<char, 256> buffer{};

  while (true) {
    sockaddr_in sender{};
#if defined(_WIN32)
    int senderLength = sizeof(sender);
    const int receivedBytes =
        recvfrom(receiver->socket, buffer.data(), static_cast<int>(buffer.size()) - 1, 0,
                 reinterpret_cast<sockaddr*>(&sender), &senderLength);
    if (receivedBytes == SOCKET_ERROR) {
      const int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK) {
        break;
      }
      return receivedFrame;
    }
#else
    socklen_t senderLength = sizeof(sender);
    const int receivedBytes =
        recvfrom(receiver->socket, buffer.data(), buffer.size() - 1, 0,
                 reinterpret_cast<sockaddr*>(&sender), &senderLength);
    if (receivedBytes < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        break;
      }
      return receivedFrame;
    }
#endif

    buffer[receivedBytes] = '\0';
    SensorFrame parsedFrame;
    if (ParsePacket(buffer.data(), &parsedFrame)) {
      *frame = parsedFrame;
      receivedFrame = true;
    }
  }

  return receivedFrame;
}

float GetAxisDegrees(DisplayAxis axis, const SensorFrame& frame) {
  return (axis == DisplayAxis::kRoll) ? frame.roll : frame.pitch;
}

const char* GetAxisLabel(DisplayAxis axis) {
  return (axis == DisplayAxis::kRoll) ? "roll" : "pitch";
}

float ClampToUnit(float value) {
  return std::clamp(value, -1.0f, 1.0f);
}

Vector2 PointOnCircle(Vector2 center, float radius, float angleDeg) {
  const float radians = angleDeg * kDegToRad;
  return Vector2{center.x + std::cos(radians) * radius,
                 center.y + std::sin(radians) * radius};
}

void DrawSteeringWheel(Vector2 center, float radius, float rotationDeg) {
  const Color rimColor = Color{46, 72, 88, 255};
  const Color spokeColor = Color{77, 92, 103, 255};
  const Color accentColor = Color{184, 72, 49, 255};
  const Color shadowColor = Color{214, 209, 196, 255};
  const Color hubColor = Color{242, 239, 228, 255};

  DrawCircleV(Vector2{center.x + 5.0f, center.y + 7.0f}, radius + 6.0f, shadowColor);
  DrawRing(center, radius * 0.76f, radius, 0.0f, 360.0f, 96, rimColor);
  DrawRing(center, radius * 0.56f, radius * 0.62f, 0.0f, 360.0f, 96,
           Color{204, 211, 214, 255});

  for (int i = 0; i < 3; ++i) {
    const float spokeAngleDeg = rotationDeg - 90.0f + static_cast<float>(i) * 120.0f;
    DrawLineEx(PointOnCircle(center, radius * 0.20f, spokeAngleDeg),
               PointOnCircle(center, radius * 0.70f, spokeAngleDeg), radius * 0.09f,
               spokeColor);
  }

  DrawCircleV(center, radius * 0.24f, rimColor);
  DrawCircleV(center, radius * 0.14f, hubColor);
  DrawCircleV(PointOnCircle(center, radius * 0.87f, rotationDeg - 90.0f), radius * 0.075f,
              accentColor);
}

std::vector<std::string> GetLocalIpv4Addresses() {
  std::vector<std::string> addresses;

#if defined(_WIN32)
  char hostName[256] = {};
  if (gethostname(hostName, sizeof(hostName)) == 0) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* info = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &info) == 0) {
      for (addrinfo* current = info; current != nullptr; current = current->ai_next) {
        const sockaddr_in* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
        char ipBuffer[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &(ipv4->sin_addr), ipBuffer, sizeof(ipBuffer)) != nullptr) {
          std::string address(ipBuffer);
          if (address != "127.0.0.1" &&
              std::find(addresses.begin(), addresses.end(), address) == addresses.end()) {
            addresses.push_back(address);
          }
        }
      }
      freeaddrinfo(info);
    }
  }
#else
  ifaddrs* interfaces = nullptr;
  if (getifaddrs(&interfaces) == 0) {
    for (ifaddrs* iface = interfaces; iface != nullptr; iface = iface->ifa_next) {
      if (iface->ifa_addr == nullptr || iface->ifa_addr->sa_family != AF_INET) {
        continue;
      }
      if ((iface->ifa_flags & IFF_UP) == 0 || (iface->ifa_flags & IFF_LOOPBACK) != 0) {
        continue;
      }

      char ipBuffer[INET_ADDRSTRLEN] = {};
      const sockaddr_in* ipv4 = reinterpret_cast<const sockaddr_in*>(iface->ifa_addr);
      if (inet_ntop(AF_INET, &(ipv4->sin_addr), ipBuffer, sizeof(ipBuffer)) != nullptr) {
        addresses.emplace_back(ipBuffer);
      }
    }
    freeifaddrs(interfaces);
  }
#endif

  if (addresses.empty()) {
    addresses.emplace_back("No active IPv4 address found");
  }

  return addresses;
}

std::string JoinLocalIps(const std::vector<std::string>& addresses) {
  std::ostringstream joined;
  for (size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0) {
      joined << ", ";
    }
    joined << addresses[i];
  }
  return joined.str();
}
}  // namespace

int main() {
  UdpReceiver receiver;
  const bool udpReady = receiver.Open(kUdpPort);
  const std::string localIpText = JoinLocalIps(GetLocalIpv4Addresses());

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(kWindowWidth, kWindowHeight, "Steering Wheel Controller POC");
  SetTargetFPS(60);

  SensorFrame latestFrame;
  SensorFrame lastGoodFrame;
  SensorFrame centerFrame;
  auto lastPacketTime = std::chrono::steady_clock::time_point{};
  DisplayAxis displayAxis = DisplayAxis::kPitch;
  bool hasCenterFrame = false;
  float manualAngleDeg = 0.0f;

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_R)) {
      displayAxis = DisplayAxis::kRoll;
    }
    if (IsKeyPressed(KEY_P)) {
      displayAxis = DisplayAxis::kPitch;
    }

    if (PollLatestSensorFrame(&receiver, &latestFrame)) {
      lastGoodFrame = latestFrame;
      lastPacketTime = std::chrono::steady_clock::now();
    }

    const float dt = GetFrameTime();
    const float keyboardDirection = static_cast<float>(IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) -
                                    static_cast<float>(IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
    if (keyboardDirection != 0.0f) {
      manualAngleDeg += keyboardDirection * kKeyboardStepPerSecond * dt;
    }
    manualAngleDeg = std::clamp(manualAngleDeg, -kVisibleAngleRangeDeg, kVisibleAngleRangeDeg);

    const auto now = std::chrono::steady_clock::now();
    const float secondsSincePacket =
        std::chrono::duration<float>(now - lastPacketTime).count();
    const bool hasFreshPackets = secondsSincePacket <= kPacketTimeoutSeconds;
    const bool hasAnyPacket = lastPacketTime != std::chrono::steady_clock::time_point{};

    float sourceAngleDeg = GetAxisDegrees(displayAxis, lastGoodFrame);
    if (!hasFreshPackets && !hasAnyPacket) {
      sourceAngleDeg = manualAngleDeg;
    }
    if (IsKeyPressed(KEY_SPACE)) {
      if (hasAnyPacket) {
        centerFrame = lastGoodFrame;
        hasCenterFrame = true;
      } else {
        manualAngleDeg = 0.0f;
        centerFrame = SensorFrame{};
        hasCenterFrame = true;
      }
    }

    const float calibrationOffsetDeg =
        hasCenterFrame ? GetAxisDegrees(displayAxis, centerFrame) : 0.0f;
    const float centeredAngleDeg = sourceAngleDeg - calibrationOffsetDeg;
    const float steeringAngleDeg = centeredAngleDeg * kSteeringDirection;
    const float normalizedValue = ClampToUnit(steeringAngleDeg / kVisibleAngleRangeDeg);

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const Vector2 center = {screenWidth * 0.5f, screenHeight * 0.52f};
    const float wheelRadius = std::min(screenWidth, screenHeight) * 0.28f;

    BeginDrawing();
    ClearBackground(Color{242, 239, 228, 255});

    DrawText("Toddler Steering Wheel POC", 40, 28, 34, Color{46, 72, 88, 255});
    DrawText("P: pitch steering   R: roll debug   SPACE: center   A/D or arrows: fallback input", 40, 70, 22,
             Color{77, 92, 103, 255});

    DrawSteeringWheel(center, wheelRadius, steeringAngleDeg);

    DrawText(TextFormat("axis: %s", GetAxisLabel(displayAxis)), 40, screenHeight - 150, 24,
             Color{46, 72, 88, 255});
    DrawText(TextFormat("raw angle: %.1f deg", sourceAngleDeg), 40, screenHeight - 118, 24,
             Color{46, 72, 88, 255});
    DrawText(TextFormat("centered angle: %.1f deg", centeredAngleDeg), 40, screenHeight - 86, 24,
             Color{46, 72, 88, 255});
    DrawText(TextFormat("steering angle: %.1f deg   normalized: %.2f", steeringAngleDeg, normalizedValue), 40, screenHeight - 54, 24,
             Color{46, 72, 88, 255});

    const char* inputMode = hasFreshPackets
                                ? "UDP sensor stream active"
                                : (hasAnyPacket ? "Showing last packet - stream stale"
                                                : "No packets yet - keyboard fallback");
    DrawText(inputMode, screenWidth - 420, 32, 22,
             hasFreshPackets ? Color{59, 120, 87, 255}
                             : (hasAnyPacket ? Color{191, 134, 33, 255}
                                             : Color{184, 72, 49, 255}));
    DrawText(TextFormat("UDP port: %d", kUdpPort), screenWidth - 420, 64, 22,
             Color{77, 92, 103, 255});
    DrawText(udpReady ? "Listener: ready" : "Listener: failed to bind UDP socket",
             screenWidth - 420, 96, 22,
             udpReady ? Color{59, 120, 87, 255} : Color{184, 72, 49, 255});
    DrawText("Host IPv4:", screenWidth - 420, 128, 22, Color{77, 92, 103, 255});
    DrawText(localIpText.c_str(), screenWidth - 420, 156, 22, Color{46, 72, 88, 255});

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
