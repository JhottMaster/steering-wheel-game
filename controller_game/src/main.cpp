#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include "platform/platform_windows.h"
#else
#include "platform/platform_linux.h"
#endif

#include "game/game_audio.h"
#include "game/game_logic.h"
#include "game/game_view.h"
#include "input/sensor_receiver.h"
#include "raylib.h"
#include "views/hardware_test_view.h"

namespace {
constexpr int kUdpPort = 4210;
constexpr float kVisibleAngleRangeDeg = 45.0f;
constexpr float kKeyboardStepPerSecond = 35.0f;
constexpr float kPacketTimeoutSeconds = 2.0f;
constexpr float kSteeringDirection = 1.0f;

enum class AppMode {
  kGame,
  kHardwareTest,
};

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
  platform::UdpReceiver receiver;
  const bool udpReady = receiver.Open(kUdpPort);
  const std::string localIpText = JoinLocalIps(platform::GetLocalIpv4Addresses());

  SetConfigFlags(platform::GetWindowConfigFlags());
  InitWindow(platform::kWindowWidth, platform::kWindowHeight, platform::kWindowTitle);
  platform::ApplyPostWindowInit();
  SetTargetFPS(60);
  InitAudioDevice();

  GameAssets gameAssets = LoadGameAssets();
  GameAudio gameAudio = LoadGameAudio();
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
    if (platform::ShouldToggleFullscreen()) {
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
    const float secondsSincePacket = std::chrono::duration<float>(now - lastPacketTime).count();
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

    const float gameSourceAngleDeg =
        hasAnyPacket ? GetAxisDegrees(DisplayAxis::kPitch, lastGoodFrame) : manualAngleDeg;
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
    UpdateGameAudio(&gameAudio, game, appMode == AppMode::kGame);

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    BeginDrawing();
    if (appMode == AppMode::kGame) {
      DrawGame(game, gameAssets, hasFreshPackets, localIpText, screenWidth, screenHeight);
      EndDrawing();
      continue;
    }

    DrawHardwareTest(lastGoodFrame, displayAxis, sourceAngleDeg, centeredAngleDeg,
                     steeringAngleDeg, normalizedValue, hasFreshPackets, hasAnyPacket, udpReady,
                     localIpText, screenWidth, screenHeight);

    EndDrawing();
  }

  UnloadGameAudio(&gameAudio);
  UnloadGameAssets(&gameAssets);
  if (IsAudioDeviceReady()) {
    CloseAudioDevice();
  }
  CloseWindow();
  return 0;
}
