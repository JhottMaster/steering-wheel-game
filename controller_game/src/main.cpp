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

#include "app/app_mode.h"
#include "app/controller_buttons.h"
#include "app/pause_menu.h"
#include "app/performance_log.h"
#include "app/server_discovery.h"
#include "game/asset_paths.h"
#include "game/game_audio.h"
#include "game/game_logic.h"
#include "game/game_view.h"
#include "game/road_art_tuning.h"
#include "input/recenter_gesture.h"
#include "input/sensor_receiver.h"
#include "input/steering_input.h"
#include "raylib.h"
#include "views/hardware_test_view.h"
#include "views/pause_menu_view.h"

namespace {
constexpr int kUdpPort = 4210;
constexpr float kVisibleAngleRangeDeg = 45.0f;
constexpr float kPacketTimeoutSeconds = 1.0f;
constexpr float kDisplaySteeringDirection = -1.0f;
constexpr float kGameSteeringDirection = -1.0f;
constexpr float kCameraZoomMin = 0.15f;
constexpr float kCameraZoomMax = 1.5f;
constexpr float kCameraZoomStep = 1.25f;

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

void PrintStartupWarnings(const char* label, const std::vector<std::string>& warnings) {
  for (const std::string& warning : warnings) {
    std::printf("[%s] %s\n", label, warning.c_str());
  }
}

void SaveRoadArtTuningWithLog(const std::string& path, const RoadArtTuning& tuning) {
  if (!SaveRoadArtTuning(path, tuning)) {
    std::printf("[road-art] failed to save tuning config: %s\n", path.c_str());
  }
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
  std::vector<std::string> cityWarnings;
  CityMap city = LoadCityMap(game_asset_paths::FindCityPath("demo_city.csv"), &cityWarnings);
  PrintStartupWarnings("city", cityWarnings);
  const std::string roadArtTuningPath = game_asset_paths::FindConfigPath("road_art_tuning.csv");
  std::vector<std::string> roadArtWarnings;
  RoadArtTuning roadArtTuning = LoadRoadArtTuning(roadArtTuningPath, &roadArtWarnings);
  PrintStartupWarnings("road-art", roadArtWarnings);
  RoadArtEditorState roadArtEditor;
  GameAudio gameAudio = LoadGameAudio();
  GameState game;
  InitializeGameFromCity(&game, &city);
  SensorFrame latestFrame;
  SensorFrame lastGoodFrame;
  auto lastPacketTime = std::chrono::steady_clock::time_point{};
  DisplayAxis displayAxis = DisplayAxis::kPitch;
  AppMode appMode = AppMode::kGame;
  SteeringInputState steeringInput;
  DiscoveryBeaconState discovery;
  RecenterGestureState recenterGesture;
  float cameraZoomScale = 1.0f;
  PauseMenuState pauseMenu;
  OpenPauseMenu(&pauseMenu);
  bool shouldQuit = false;
  PerformanceWindow performance;

  while (!shouldQuit && !WindowShouldClose()) {
    const auto frameStartTime = PerfClock::now();
    auto inputStartTime = frameStartTime;
    if (platform::ShouldToggleFullscreen()) {
      ToggleFullscreen();
    }
    if (!pauseMenu.active && IsKeyPressed(KEY_T)) {
      appMode = appMode == AppMode::kGame ? AppMode::kHardwareTest : AppMode::kGame;
    }
    if (!pauseMenu.active && appMode == AppMode::kGame && IsKeyPressed(KEY_A)) {
      ToggleDriveMode(&game);
    }
    if (!pauseMenu.active && appMode == AppMode::kGame && IsKeyPressed(KEY_E)) {
      roadArtEditor.active = !roadArtEditor.active;
      if (!roadArtEditor.active) {
        SaveRoadArtTuningWithLog(roadArtTuningPath, roadArtTuning);
      }
    }
    if (!pauseMenu.active && appMode == AppMode::kGame &&
        (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))) {
      cameraZoomScale = std::max(kCameraZoomMin, cameraZoomScale / kCameraZoomStep);
    }
    if (!pauseMenu.active && appMode == AppMode::kGame &&
        (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))) {
      cameraZoomScale = std::min(kCameraZoomMax, cameraZoomScale * kCameraZoomStep);
    }
    if (!pauseMenu.active && appMode == AppMode::kGame &&
        (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0))) {
      cameraZoomScale = 1.0f;
    }
    if (!pauseMenu.active && appMode == AppMode::kGame && roadArtEditor.active) {
      if (IsKeyPressed(KEY_GRAVE)) {
        roadArtEditor.selectingAsset = !roadArtEditor.selectingAsset;
      }
      if (IsKeyPressed(KEY_UP)) {
        if (roadArtEditor.selectingAsset) {
          AdjustRoadArtEditorSprite(&roadArtEditor, -1);
        } else {
          roadArtEditor.fieldIndex = (roadArtEditor.fieldIndex + 5) % 6;
        }
      }
      if (IsKeyPressed(KEY_DOWN)) {
        if (roadArtEditor.selectingAsset) {
          AdjustRoadArtEditorSprite(&roadArtEditor, 1);
        } else {
          roadArtEditor.fieldIndex = (roadArtEditor.fieldIndex + 1) % 6;
        }
      }
      bool changedRoadArtTuning = false;
      if (IsKeyPressed(KEY_RIGHT)) {
        AdjustRoadArtEditorValue(&roadArtTuning, roadArtEditor, 1);
        changedRoadArtTuning = true;
      }
      if (IsKeyPressed(KEY_LEFT)) {
        AdjustRoadArtEditorValue(&roadArtTuning, roadArtEditor, -1);
        changedRoadArtTuning = true;
      }
      if (changedRoadArtTuning) {
        SaveRoadArtTuningWithLog(roadArtTuningPath, roadArtTuning);
      }
    }

    if (!pauseMenu.active && appMode == AppMode::kHardwareTest && IsKeyPressed(KEY_R)) {
      displayAxis = DisplayAxis::kRoll;
    }
    if (!pauseMenu.active && appMode == AppMode::kHardwareTest && IsKeyPressed(KEY_P)) {
      displayAxis = DisplayAxis::kPitch;
    }
    if (!pauseMenu.active && appMode == AppMode::kHardwareTest && IsKeyPressed(KEY_Y)) {
      displayAxis = DisplayAxis::kYaw;
    }

    if (PollLatestSensorFrame(&receiver, &latestFrame)) {
      lastGoodFrame = latestFrame;
      lastPacketTime = std::chrono::steady_clock::now();
      MarkControllerPacketReceived(&discovery);
      RecordSensorFrameForSteering(lastGoodFrame, &steeringInput);
    }

    const float dt = GetFrameTime();
    const auto inputEndTime = PerfClock::now();
    const auto now = std::chrono::steady_clock::now();
    const float secondsSincePacket = std::chrono::duration<float>(now - lastPacketTime).count();
    const bool hasFreshPackets = secondsSincePacket <= kPacketTimeoutSeconds;
    const bool hasAnyPacket = lastPacketTime != std::chrono::steady_clock::time_point{};
    const bool roadArtEditorConsumesArrows = appMode == AppMode::kGame && roadArtEditor.active;
    const float rawKeyboardDirection =
        roadArtEditorConsumesArrows
            ? 0.0f
            : static_cast<float>(IsKeyDown(KEY_LEFT)) - static_cast<float>(IsKeyDown(KEY_RIGHT));
    const float keyboardTravelDirection = game.carSpeed < 0.0f ? -1.0f : 1.0f;
    const float keyboardDirection = rawKeyboardDirection * keyboardTravelDirection;
    UpdateKeyboardSteeringFallback(&steeringInput, keyboardDirection, hasFreshPackets, dt);

    UpdateServerDiscoveryBeacon(&discovery, &broadcaster, broadcastReady, hasFreshPackets,
                                lastPacketTime, now);

    const ControllerButtonState menuButtons =
        ReadControllerButtons(lastGoodFrame, hasFreshPackets);
    const bool pauseChordDown = menuButtons.green && menuButtons.red;
    const float pauseMenuSteeringAngleDeg =
        hasAnyPacket ? GetWrappedGameWheelAngleDeg(lastGoodFrame, steeringInput) *
                           kGameSteeringDirection
                     : steeringInput.manualAngleDeg;
    const PauseMenuAction pauseMenuAction =
        UpdatePauseMenu(&pauseMenu, pauseMenuSteeringAngleDeg, menuButtons.green,
                        menuButtons.red, dt);

    switch (pauseMenuAction) {
      case PauseMenuAction::kResume:
        ClosePauseMenu(&pauseMenu);
        break;
      case PauseMenuAction::kRestart:
        game = GameState{};
        InitializeGameFromCity(&game, &city);
        ClosePauseMenu(&pauseMenu);
        break;
      case PauseMenuAction::kCenter:
        ResetControllerCenter(lastGoodFrame, hasAnyPacket, &steeringInput);
        break;
      case PauseMenuAction::kQuit:
        shouldQuit = true;
        break;
      case PauseMenuAction::kToggleDriveMode:
        ToggleDriveMode(&game);
        break;
      case PauseMenuAction::kToggleHardwareTest:
        appMode = appMode == AppMode::kGame ? AppMode::kHardwareTest : AppMode::kGame;
        ClosePauseMenu(&pauseMenu);
        break;
      case PauseMenuAction::kZoomIn:
        cameraZoomScale = std::min(kCameraZoomMax, cameraZoomScale * kCameraZoomStep);
        break;
      case PauseMenuAction::kZoomOut:
        cameraZoomScale = std::max(kCameraZoomMin, cameraZoomScale / kCameraZoomStep);
        break;
      case PauseMenuAction::kNone:
        break;
    }

    if (UpdateRecenterGesture(&recenterGesture, menuButtons, pauseMenu.active, pauseChordDown,
                              now)) {
      ResetControllerCenter(lastGoodFrame, true, &steeringInput);
    }

    if (IsKeyPressed(KEY_SPACE)) {
      ResetControllerCenter(lastGoodFrame, hasAnyPacket, &steeringInput);
      ResetRecenterGesture(&recenterGesture);
    }

    const float sourceAngleDeg =
        hasAnyPacket ? GetAxisDegrees(displayAxis, lastGoodFrame) : steeringInput.manualAngleDeg;
    const float centeredAngleDeg =
        hasAnyPacket && steeringInput.hasCenterFrame
            ? GetCenteredAxisDegrees(displayAxis, lastGoodFrame, steeringInput.centerFrame)
            : sourceAngleDeg;
    const float steeringAngleDeg = centeredAngleDeg * kDisplaySteeringDirection;
    const float normalizedValue = ClampUnit(steeringAngleDeg / kVisibleAngleRangeDeg);

    const float gameSourceAngleDeg =
        hasAnyPacket ? steeringInput.accumulatedGameAngleDeg : steeringInput.manualAngleDeg;
    const float gameSteeringInput =
        ClampUnit(ApplyGameSteeringResponseCurve(gameSourceAngleDeg * kGameSteeringDirection));
    const ControllerButtonState gameInputButtons =
        ReadControllerButtons(lastGoodFrame, hasFreshPackets, !roadArtEditorConsumesArrows);
    const GameButtons gameButtons = {
        gameInputButtons.green,
        gameInputButtons.red,
    };
    const auto updateStartTime = PerfClock::now();
    if (appMode == AppMode::kGame && !pauseMenu.active) {
      UpdateGame(&game, gameSteeringInput, gameButtons, dt, &city);
    }
    const auto updateEndTime = PerfClock::now();
    const auto audioStartTime = updateEndTime;
    UpdateGameAudio(&gameAudio, game, appMode == AppMode::kGame && !pauseMenu.active);
    const auto audioEndTime = PerfClock::now();

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    const auto drawStartTime = PerfClock::now();
    BeginDrawing();
    if (appMode == AppMode::kGame) {
      DrawGame(game, gameAssets, city, roadArtTuning, roadArtEditor, hasFreshPackets,
               hasAnyPacket, discovery.wasBroadcastingForLoss, screenWidth, screenHeight,
               cameraZoomScale);
      if (pauseMenu.active) {
        DrawPauseMenu(pauseMenu, game, appMode, localIpText, cameraZoomScale, screenWidth,
                      screenHeight);
      }
      EndDrawing();
      const auto frameEndTime = PerfClock::now();
      AddPerformanceSample(
          &performance, ElapsedMilliseconds(frameStartTime, frameEndTime),
          static_cast<double>(dt) * 1000.0, ElapsedMilliseconds(inputStartTime, inputEndTime),
          ElapsedMilliseconds(updateStartTime, updateEndTime),
          ElapsedMilliseconds(audioStartTime, audioEndTime),
          ElapsedMilliseconds(drawStartTime, frameEndTime));
      MaybeLogPerformance(&performance, appMode, pauseMenu.active);
      continue;
    }

    DrawHardwareTest(lastGoodFrame, displayAxis, sourceAngleDeg, centeredAngleDeg,
                     steeringAngleDeg, normalizedValue, menuButtons, hasFreshPackets,
                     hasAnyPacket, udpReady, localIpText, screenWidth, screenHeight);
    if (pauseMenu.active) {
      DrawPauseMenu(pauseMenu, game, appMode, localIpText, cameraZoomScale, screenWidth,
                    screenHeight);
    }

    EndDrawing();
    const auto frameEndTime = PerfClock::now();
    AddPerformanceSample(
        &performance, ElapsedMilliseconds(frameStartTime, frameEndTime),
        static_cast<double>(dt) * 1000.0, ElapsedMilliseconds(inputStartTime, inputEndTime),
        ElapsedMilliseconds(updateStartTime, updateEndTime),
        ElapsedMilliseconds(audioStartTime, audioEndTime),
        ElapsedMilliseconds(drawStartTime, frameEndTime));
    MaybeLogPerformance(&performance, appMode, pauseMenu.active);
  }

  UnloadGameAudio(&gameAudio);
  UnloadGameAssets(&gameAssets);
  if (IsAudioDeviceReady()) {
    CloseAudioDevice();
  }
  CloseWindow();
  return 0;
}
