#include <algorithm>
#include <chrono>
#include <cstdio>
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
constexpr int kServerDiscoveryPort = 4211;
constexpr float kServerBeaconIntervalSeconds = 3.0f;
constexpr float kServerBeaconStartDelaySeconds = 5.0f;
constexpr float kVisibleAngleRangeDeg = 45.0f;
constexpr float kGameSteeringRangeDeg = 65.0f;
constexpr float kKeyboardStepPerSecond = 35.0f;
constexpr float kPacketTimeoutSeconds = 2.0f;
constexpr float kDisplaySteeringDirection = -1.0f;
constexpr float kGameSteeringDirection = -1.0f;
constexpr float kRecenterChordHoldSeconds = 5.0f;
constexpr char kServerDiscoveryMessage[] = "steering-wheel-server port=4210";

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
  platform::UdpBroadcaster broadcaster;
  const bool udpReady = receiver.Open(kUdpPort);
  const bool broadcastReady = broadcaster.Open();
  const std::string localIpText = JoinLocalIps(platform::GetLocalIpv4Addresses());
  const auto appStartTime = std::chrono::steady_clock::now();

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
  auto lastBeaconTime = std::chrono::steady_clock::time_point{};
  bool hasEverReceivedPacket = false;
  bool wasBroadcastingForLoss = false;
  bool simultaneousButtonsWereDown = false;
  auto simultaneousButtonsDownSince = std::chrono::steady_clock::time_point{};
  bool recenterChordTriggered = false;

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
      hasEverReceivedPacket = true;
      wasBroadcastingForLoss = false;
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
    const float secondsSinceBeacon =
        lastBeaconTime == std::chrono::steady_clock::time_point{}
            ? kServerBeaconIntervalSeconds
            : std::chrono::duration<float>(now - lastBeaconTime).count();

    const bool shouldStartInitialBroadcast = !hasEverReceivedPacket;
    const bool shouldStartLossBroadcast =
        hasEverReceivedPacket && !hasFreshPackets &&
        std::chrono::duration<float>(now - lastPacketTime).count() >= kServerBeaconStartDelaySeconds;
    const bool shouldBroadcast = shouldStartInitialBroadcast || shouldStartLossBroadcast;

    if (shouldStartLossBroadcast && !wasBroadcastingForLoss) {
      lastBeaconTime = std::chrono::steady_clock::time_point{};
      wasBroadcastingForLoss = true;
    }

    if (broadcastReady && shouldBroadcast && secondsSinceBeacon >= kServerBeaconIntervalSeconds) {
      broadcaster.SendBroadcast(kServerDiscoveryMessage,
                                static_cast<int>(sizeof(kServerDiscoveryMessage) - 1),
                                kServerDiscoveryPort);
      std::printf("Server discovery broadcast sent to UDP %d: %s\n", kServerDiscoveryPort,
                  kServerDiscoveryMessage);
      lastBeaconTime = now;
    }

    const bool simultaneousButtonsDown =
        hasFreshPackets && lastGoodFrame.button1Pressed && lastGoodFrame.button2Pressed;
    if (simultaneousButtonsDown && !simultaneousButtonsWereDown) {
      simultaneousButtonsDownSince = now;
      recenterChordTriggered = false;
      std::printf("Recenter chord started. Hold both buttons for %.1f seconds.\n",
                  kRecenterChordHoldSeconds);
    }
    if (simultaneousButtonsDown && !recenterChordTriggered &&
        simultaneousButtonsDownSince != std::chrono::steady_clock::time_point{} &&
        std::chrono::duration<float>(now - simultaneousButtonsDownSince).count() >=
            kRecenterChordHoldSeconds) {
      if (hasAnyPacket) {
        centerFrame = lastGoodFrame;
        hasCenterFrame = true;
        std::printf("Controller center reset after %.1f second simultaneous button hold.\n",
                    kRecenterChordHoldSeconds);
      }
      recenterChordTriggered = true;
    }
    if (!simultaneousButtonsDown && simultaneousButtonsWereDown) {
      const float heldSeconds =
          simultaneousButtonsDownSince == std::chrono::steady_clock::time_point{}
              ? 0.0f
              : std::chrono::duration<float>(now - simultaneousButtonsDownSince).count();
      if (!recenterChordTriggered) {
        std::printf("Recenter chord released after %.2f seconds.\n", heldSeconds);
      }
      simultaneousButtonsDownSince = std::chrono::steady_clock::time_point{};
      recenterChordTriggered = false;
    }
    simultaneousButtonsWereDown = simultaneousButtonsDown;

    if (IsKeyPressed(KEY_SPACE)) {
      if (hasAnyPacket) {
        centerFrame = lastGoodFrame;
        hasCenterFrame = true;
      } else {
        manualAngleDeg = 0.0f;
        hasCenterFrame = false;
      }
      simultaneousButtonsDownSince = std::chrono::steady_clock::time_point{};
      recenterChordTriggered = false;
    }

    const float sourceAngleDeg =
        hasAnyPacket ? GetAxisDegrees(displayAxis, lastGoodFrame) : manualAngleDeg;
    const float centeredAngleDeg =
        hasAnyPacket && hasCenterFrame
            ? GetCenteredAxisDegrees(displayAxis, lastGoodFrame, centerFrame)
            : sourceAngleDeg;
    const float steeringAngleDeg = centeredAngleDeg * kDisplaySteeringDirection;
    const float normalizedValue = ClampUnit(steeringAngleDeg / kVisibleAngleRangeDeg);

    const float gameSourceAngleDeg =
        hasAnyPacket && hasCenterFrame
            ? GetCenteredAxisDegrees(DisplayAxis::kPitch, lastGoodFrame, centerFrame)
            : (hasAnyPacket ? GetAxisDegrees(DisplayAxis::kPitch, lastGoodFrame) : manualAngleDeg);
    const float gameSteeringInput =
        ClampUnit((gameSourceAngleDeg * kGameSteeringDirection) / kGameSteeringRangeDeg);
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
      DrawGame(game, gameAssets, hasFreshPackets, hasAnyPacket, wasBroadcastingForLoss,
               localIpText, screenWidth, screenHeight);
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
