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
constexpr float kGameSteeringFullLockDeg = 270.0f;
constexpr float kKeyboardStepPerSecond = 35.0f;
constexpr float kPacketTimeoutSeconds = 2.0f;
constexpr float kDisplaySteeringDirection = -1.0f;
constexpr float kGameSteeringDirection = -1.0f;
constexpr float kRecenterGestureWindowSeconds = 1.5f;
constexpr int kRecenterRedPressesRequired = 3;
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

float GetWrappedGameWheelAngleDeg(const SensorFrame& frame, const SensorFrame& centerFrame,
                                  bool hasCenterFrame) {
  return hasCenterFrame ? GetCenteredAxisDegrees(DisplayAxis::kPitch, frame, centerFrame)
                        : GetAxisDegrees(DisplayAxis::kPitch, frame);
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
  bool greenButtonGestureActive = false;
  auto greenButtonGestureStartTime = std::chrono::steady_clock::time_point{};
  bool redButtonWasDown = false;
  int redButtonPressCount = 0;
  float gameAccumulatedAngleDeg = 0.0f;
  float lastWrappedGameAngleDeg = 0.0f;
  bool hasLastWrappedGameAngle = false;

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

      const float wrappedGameAngleDeg =
          GetWrappedGameWheelAngleDeg(lastGoodFrame, centerFrame, hasCenterFrame);
      if (!hasLastWrappedGameAngle) {
        gameAccumulatedAngleDeg = wrappedGameAngleDeg;
        hasLastWrappedGameAngle = true;
      } else {
        gameAccumulatedAngleDeg += WrapDegrees(wrappedGameAngleDeg - lastWrappedGameAngleDeg);
      }
      lastWrappedGameAngleDeg = wrappedGameAngleDeg;
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

    const bool greenButtonDown = hasFreshPackets && lastGoodFrame.button1Pressed;
    const bool redButtonDown = hasFreshPackets && lastGoodFrame.button2Pressed;

    if (greenButtonDown && !greenButtonGestureActive) {
      greenButtonGestureActive = true;
      greenButtonGestureStartTime = now;
      redButtonPressCount = 0;
      redButtonWasDown = redButtonDown;
      std::printf("Recenter gesture started. Hold green and press red %d times within %.1f seconds.\n",
                  kRecenterRedPressesRequired, kRecenterGestureWindowSeconds);
    }

    if (greenButtonGestureActive) {
      const float gestureElapsedSeconds =
          std::chrono::duration<float>(now - greenButtonGestureStartTime).count();

      if (redButtonDown && !redButtonWasDown) {
        ++redButtonPressCount;
        std::printf("Recenter gesture red press %d/%d.\n", redButtonPressCount,
                    kRecenterRedPressesRequired);
      }
      redButtonWasDown = redButtonDown;

      if (!greenButtonDown) {
        std::printf("Recenter gesture cancelled: green released.\n");
        greenButtonGestureActive = false;
        greenButtonGestureStartTime = std::chrono::steady_clock::time_point{};
        redButtonPressCount = 0;
        redButtonWasDown = false;
      } else if (gestureElapsedSeconds > kRecenterGestureWindowSeconds) {
        if (redButtonPressCount >= kRecenterRedPressesRequired) {
          centerFrame = lastGoodFrame;
          hasCenterFrame = true;
          gameAccumulatedAngleDeg = 0.0f;
          lastWrappedGameAngleDeg = 0.0f;
          hasLastWrappedGameAngle = false;
          std::printf("Controller center reset from green hold + red triple press gesture.\n");
        } else {
          std::printf("Recenter gesture timed out at %.2f seconds with %d/%d red presses.\n",
                      gestureElapsedSeconds, redButtonPressCount, kRecenterRedPressesRequired);
        }
        greenButtonGestureActive = false;
        greenButtonGestureStartTime = std::chrono::steady_clock::time_point{};
        redButtonPressCount = 0;
        redButtonWasDown = false;
      }
    }

    if (IsKeyPressed(KEY_SPACE)) {
      if (hasAnyPacket) {
        centerFrame = lastGoodFrame;
        hasCenterFrame = true;
        gameAccumulatedAngleDeg = 0.0f;
        lastWrappedGameAngleDeg = 0.0f;
        hasLastWrappedGameAngle = false;
      } else {
        manualAngleDeg = 0.0f;
        hasCenterFrame = false;
        gameAccumulatedAngleDeg = 0.0f;
        lastWrappedGameAngleDeg = 0.0f;
        hasLastWrappedGameAngle = false;
      }
      greenButtonGestureActive = false;
      greenButtonGestureStartTime = std::chrono::steady_clock::time_point{};
      redButtonPressCount = 0;
      redButtonWasDown = false;
    }

    const float sourceAngleDeg =
        hasAnyPacket ? GetAxisDegrees(displayAxis, lastGoodFrame) : manualAngleDeg;
    const float centeredAngleDeg =
        hasAnyPacket && hasCenterFrame
            ? GetCenteredAxisDegrees(displayAxis, lastGoodFrame, centerFrame)
            : sourceAngleDeg;
    const float steeringAngleDeg = centeredAngleDeg * kDisplaySteeringDirection;
    const float normalizedValue = ClampUnit(steeringAngleDeg / kVisibleAngleRangeDeg);

    const float gameSourceAngleDeg = hasAnyPacket ? gameAccumulatedAngleDeg : manualAngleDeg;
    const float gameSteeringInput =
        ClampUnit((gameSourceAngleDeg * kGameSteeringDirection) / kGameSteeringFullLockDeg);
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
