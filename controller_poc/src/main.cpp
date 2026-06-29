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
#include "game_logic.h"

namespace {
constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;
constexpr int kUdpPort = 4210;
constexpr float kVisibleAngleRangeDeg = 45.0f;
constexpr float kKeyboardStepPerSecond = 35.0f;
constexpr float kPacketTimeoutSeconds = 2.0f;
constexpr float kDegToRad = 0.017453292519943295769f;
constexpr float kSteeringDirection = 1.0f;

enum class DisplayAxis {
  kRoll,
  kPitch,
  kYaw,
};

enum class AppMode {
  kGame,
  kHardwareTest,
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
  switch (axis) {
    case DisplayAxis::kRoll:
      return frame.roll;
    case DisplayAxis::kPitch:
      return frame.pitch;
    case DisplayAxis::kYaw:
      return frame.heading;
  }

  return frame.pitch;
}

const char* GetAxisLabel(DisplayAxis axis) {
  switch (axis) {
    case DisplayAxis::kRoll:
      return "roll";
    case DisplayAxis::kPitch:
      return "pitch";
    case DisplayAxis::kYaw:
      return "yaw";
  }

  return "pitch";
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

void DrawButtonLamp(Vector2 center, float radius, bool pressed, Color dimColor, Color litColor,
                    const char* label) {
  const Color lampColor = pressed ? litColor : dimColor;
  DrawCircleV(Vector2{center.x + 4.0f, center.y + 5.0f}, radius + 4.0f,
              Color{38, 46, 49, 85});
  DrawCircleV(center, radius + 5.0f, Color{46, 72, 88, 255});
  DrawCircleV(center, radius, lampColor);
  if (pressed) {
    DrawCircleV(Vector2{center.x - radius * 0.25f, center.y - radius * 0.25f},
                radius * 0.30f, Color{255, 255, 255, 95});
  }

  const int labelWidth = MeasureText(label, 20);
  DrawText(label, static_cast<int>(center.x - labelWidth / 2), static_cast<int>(center.y + radius + 12.0f),
           20, Color{46, 72, 88, 255});
}

struct GameAssets {
  Texture2D map = {};
  Texture2D car = {};
  Texture2D coin = {};
};

bool TextureLoaded(Texture2D texture) {
  return texture.id != 0;
}

std::string FindSpritePath(const char* filename) {
  const std::vector<std::string> candidates = {
      std::string("assets/sprites/") + filename,
      std::string("controller_poc/assets/sprites/") + filename,
      std::string("../assets/sprites/") + filename,
      std::string("../../assets/sprites/") + filename,
  };

  for (const std::string& candidate : candidates) {
    if (FileExists(candidate.c_str())) {
      return candidate;
    }
  }

  return candidates.front();
}

GameAssets LoadGameAssets() {
  GameAssets assets;
  assets.map = LoadTexture(FindSpritePath("road_carpet_map_2.png").c_str());
  assets.car = LoadTexture(FindSpritePath("sports_car_top.png").c_str());
  assets.coin = LoadTexture(FindSpritePath("coin.png").c_str());
  return assets;
}

void UnloadGameAssets(GameAssets* assets) {
  if (TextureLoaded(assets->map)) {
    UnloadTexture(assets->map);
  }
  if (TextureLoaded(assets->car)) {
    UnloadTexture(assets->car);
  }
  if (TextureLoaded(assets->coin)) {
    UnloadTexture(assets->coin);
  }
}

Vector2 ToVector2(GameVec2 value) {
  return Vector2{value.x, value.y};
}

Camera2D BuildGameCamera(const GameState& game, int screenWidth, int screenHeight) {
  constexpr float targetWorldWidth = 640.0f;
  constexpr float minZoom = 1.0f;
  constexpr float maxZoom = 2.4f;
  const float zoom =
      std::clamp(static_cast<float>(screenWidth) / targetWorldWidth, minZoom, maxZoom);
  const float halfViewWidth = static_cast<float>(screenWidth) * 0.5f / zoom;
  const float halfViewHeight = static_cast<float>(screenHeight) * 0.5f / zoom;
  Camera2D camera = {};
  camera.offset = Vector2{static_cast<float>(screenWidth) * 0.5f, static_cast<float>(screenHeight) * 0.5f};
  if (halfViewWidth * 2.0f >= kGameMapSize) {
    camera.target.x = kGameMapSize * 0.5f;
  } else {
    camera.target.x = std::clamp(game.carPosition.x, halfViewWidth, kGameMapSize - halfViewWidth);
  }
  if (halfViewHeight * 2.0f >= kGameMapSize) {
    camera.target.y = kGameMapSize * 0.5f;
  } else {
    camera.target.y = std::clamp(game.carPosition.y, halfViewHeight, kGameMapSize - halfViewHeight);
  }
  camera.rotation = 0.0f;
  camera.zoom = zoom;
  return camera;
}

void DrawGame(const GameState& game, const GameAssets& assets, bool hasFreshPackets,
              const std::string& localIpText, int screenWidth, int screenHeight) {
  ClearBackground(Color{60, 133, 126, 255});

  const Camera2D camera = BuildGameCamera(game, screenWidth, screenHeight);
  BeginMode2D(camera);
  if (TextureLoaded(assets.map)) {
    DrawTexture(assets.map, 0, 0, WHITE);
  } else {
    DrawRectangle(0, 0, static_cast<int>(kGameMapSize), static_cast<int>(kGameMapSize),
                  Color{68, 142, 134, 255});
  }

  for (const CoinState& coin : game.coins) {
    if (coin.collected) {
      continue;
    }
    if (TextureLoaded(assets.coin)) {
      const Rectangle source = {0.0f, 0.0f, static_cast<float>(assets.coin.width),
                                static_cast<float>(assets.coin.height)};
      const Rectangle destination = {coin.position.x, coin.position.y, 46.0f, 46.0f};
      DrawTexturePro(assets.coin, source, destination, Vector2{23.0f, 23.0f}, 0.0f, WHITE);
    } else {
      DrawCircleV(ToVector2(coin.position), 22.0f, GOLD);
    }
  }

  if (TextureLoaded(assets.car)) {
    const Rectangle source = {0.0f, 0.0f, static_cast<float>(assets.car.width),
                              static_cast<float>(assets.car.height)};
    const float carDrawWidth = 58.0f;
    const float carDrawHeight = carDrawWidth * static_cast<float>(assets.car.height) /
                                static_cast<float>(assets.car.width);
    const Rectangle destination = {game.carPosition.x, game.carPosition.y, carDrawWidth, carDrawHeight};
    DrawTexturePro(assets.car, source, destination,
                   Vector2{carDrawWidth * 0.5f, carDrawHeight * 0.5f}, game.carHeadingDeg, WHITE);
  } else {
    const Vector2 nose = PointOnCircle(ToVector2(game.carPosition), 34.0f, game.carHeadingDeg - 90.0f);
    DrawCircleV(ToVector2(game.carPosition), 28.0f, RED);
    DrawCircleV(nose, 8.0f, YELLOW);
  }
  EndMode2D();

  DrawRectangle(20, 20, 420, 122, Color{18, 28, 32, 185});
  DrawText("Road Carpet Drive", 38, 36, 30, Color{255, 244, 205, 255});
  DrawText(TextFormat("coins: %d / %d", game.score, static_cast<int>(game.coins.size())),
           40, 74, 22, Color{232, 236, 224, 255});
  DrawText(TextFormat("drive: %s   speed: %.0f",
                      game.driveMode == DriveMode::kAuto ? "auto" : "button",
                      game.carSpeed),
           40, 104, 20, Color{232, 236, 224, 255});

  DrawRectangle(screenWidth - 430, 20, 410, 118, Color{18, 28, 32, 185});
  DrawText("T: hardware test   F11: fullscreen", screenWidth - 408, 36, 22, Color{232, 236, 224, 255});
  DrawText("A: auto/button drive   SPACE: center", screenWidth - 408, 66, 19,
           Color{195, 214, 204, 255});
  DrawText(hasFreshPackets ? "UDP controller active" : "keyboard fallback",
           screenWidth - 408, 94, 19,
           hasFreshPackets ? Color{104, 230, 141, 255} : Color{246, 187, 87, 255});
  DrawText(localIpText.c_str(), screenWidth - 408, 116, 16, Color{176, 196, 186, 255});
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

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
  InitWindow(kWindowWidth, kWindowHeight, "Steering Wheel Controller POC");
  SetWindowMinSize(960, 540);
  MaximizeWindow();
  SetTargetFPS(60);

  GameAssets gameAssets = LoadGameAssets();
  GameState game;
  SensorFrame latestFrame;
  SensorFrame lastGoodFrame;
  SensorFrame centerFrame;
  auto lastPacketTime = std::chrono::steady_clock::time_point{};
  DisplayAxis displayAxis = DisplayAxis::kPitch;
  AppMode appMode = AppMode::kGame;
  bool hasCenterFrame = false;
  float manualAngleDeg = 0.0f;

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_F11)) {
      ToggleFullscreen();
    }
    if (IsKeyPressed(KEY_T)) {
      appMode = appMode == AppMode::kGame ? AppMode::kHardwareTest : AppMode::kGame;
    }
    if (appMode == AppMode::kGame && IsKeyPressed(KEY_A)) {
      ToggleDriveMode(&game);
    }

    if (appMode == AppMode::kHardwareTest && IsKeyPressed(KEY_R)) {
      displayAxis = DisplayAxis::kRoll;
    }
    if (appMode == AppMode::kHardwareTest && IsKeyPressed(KEY_P)) {
      displayAxis = DisplayAxis::kPitch;
    }
    if (appMode == AppMode::kHardwareTest && IsKeyPressed(KEY_Y)) {
      displayAxis = DisplayAxis::kYaw;
    }

    if (PollLatestSensorFrame(&receiver, &latestFrame)) {
      lastGoodFrame = latestFrame;
      lastPacketTime = std::chrono::steady_clock::now();
      if (!hasCenterFrame) {
        centerFrame = lastGoodFrame;
        hasCenterFrame = true;
      }
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
    const float normalizedValue = ClampUnit(steeringAngleDeg / kVisibleAngleRangeDeg);

    const float gameSourceAngleDeg = hasAnyPacket ? GetAxisDegrees(DisplayAxis::kPitch, lastGoodFrame)
                                                  : manualAngleDeg;
    const float gameCalibrationOffsetDeg =
        hasCenterFrame ? GetAxisDegrees(DisplayAxis::kPitch, centerFrame) : 0.0f;
    const float gameSteeringInput =
        ClampUnit(((gameSourceAngleDeg - gameCalibrationOffsetDeg) * kSteeringDirection) /
                  kVisibleAngleRangeDeg);
    const GameButtons gameButtons = {
        (hasFreshPackets && lastGoodFrame.button1Pressed) ||
            (!hasFreshPackets && (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))),
        (hasFreshPackets && lastGoodFrame.button2Pressed) ||
            (!hasFreshPackets && (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))),
    };
    if (appMode == AppMode::kGame) {
      UpdateGame(&game, gameSteeringInput, gameButtons, dt);
    }

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const Vector2 center = {screenWidth * 0.5f, screenHeight * 0.52f};
    const float wheelRadius = std::min(screenWidth, screenHeight) * 0.28f;

    BeginDrawing();
    if (appMode == AppMode::kGame) {
      DrawGame(game, gameAssets, hasFreshPackets, localIpText, screenWidth, screenHeight);
      EndDrawing();
      continue;
    }

    ClearBackground(Color{242, 239, 228, 255});
    DrawText("Toddler Steering Wheel POC", 40, 28, 34, Color{46, 72, 88, 255});
    DrawText("T: game/test   F11: fullscreen   P/R/Y: axis   SPACE: center   A/D or arrows: fallback input", 40, 70, 22,
             Color{77, 92, 103, 255});

    DrawSteeringWheel(center, wheelRadius, steeringAngleDeg);
    DrawButtonLamp(Vector2{center.x - wheelRadius * 1.35f, center.y}, wheelRadius * 0.16f,
                   lastGoodFrame.button2Pressed, Color{92, 35, 34, 255},
                   Color{237, 54, 43, 255}, "button 2");
    DrawButtonLamp(Vector2{center.x + wheelRadius * 1.35f, center.y}, wheelRadius * 0.16f,
                   lastGoodFrame.button1Pressed, Color{35, 84, 50, 255},
                   Color{52, 222, 98, 255}, "button 1");

    DrawText(TextFormat("axis: %s", GetAxisLabel(displayAxis)), 40, screenHeight - 150, 24,
             Color{46, 72, 88, 255});
    DrawText(TextFormat("raw angle: %.1f deg", sourceAngleDeg), 40, screenHeight - 118, 24,
             Color{46, 72, 88, 255});
    DrawText(TextFormat("centered angle: %.1f deg", centeredAngleDeg), 40, screenHeight - 86, 24,
             Color{46, 72, 88, 255});
    DrawText(TextFormat("steering angle: %.1f deg   normalized: %.2f", steeringAngleDeg, normalizedValue), 40, screenHeight - 54, 24,
             Color{46, 72, 88, 255});
    DrawText(TextFormat("buttons: 1=%s  2=%s",
                        lastGoodFrame.button1Pressed ? "pressed" : "up",
                        lastGoodFrame.button2Pressed ? "pressed" : "up"),
             40, screenHeight - 22, 20, Color{46, 72, 88, 255});

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
    DrawText("Latest packet:", screenWidth - 420, 200, 22, Color{77, 92, 103, 255});
    DrawText(TextFormat("roll %.1f  pitch %.1f  heading %.1f",
                        lastGoodFrame.roll, lastGoodFrame.pitch, lastGoodFrame.heading),
             screenWidth - 420, 228, 22, Color{46, 72, 88, 255});
    DrawText(TextFormat("button1 %s  button2 %s",
                        lastGoodFrame.button1Pressed ? "pressed" : "up",
                        lastGoodFrame.button2Pressed ? "pressed" : "up"),
             screenWidth - 420, 256, 22, Color{46, 72, 88, 255});

    EndDrawing();
  }

  UnloadGameAssets(&gameAssets);
  CloseWindow();
  return 0;
}
