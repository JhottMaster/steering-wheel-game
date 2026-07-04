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

struct AppRuntime {
  GameAssets gameAssets;
  CityMap city;
  std::string roadArtTuningPath;
  RoadArtTuning roadArtTuning;
  RoadArtEditorState roadArtEditor;
  GameAudio gameAudio;
  GameState game;
  SensorFrame latestFrame;
  SensorFrame lastGoodFrame;
  std::chrono::steady_clock::time_point lastPacketTime = {};
  DisplayAxis displayAxis = DisplayAxis::kPitch;
  AppMode appMode = AppMode::kGame;
  SteeringInputState steeringInput;
  DiscoveryBeaconState discovery;
  RecenterGestureState recenterGesture;
  float cameraZoomScale = 1.0f;
  PauseMenuState pauseMenu;
  bool shouldQuit = false;
  PerformanceWindow performance;
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

void HandleRoadArtEditorInput(RoadArtEditorState* roadArtEditor, RoadArtTuning* roadArtTuning,
                              const std::string& roadArtTuningPath) {
  if (!roadArtEditor->active) {
    return;
  }

  if (IsKeyPressed(KEY_GRAVE)) {
    roadArtEditor->selectingAsset = !roadArtEditor->selectingAsset;
  }
  if (IsKeyPressed(KEY_UP)) {
    if (roadArtEditor->selectingAsset) {
      AdjustRoadArtEditorSprite(roadArtEditor, -1);
    } else {
      roadArtEditor->fieldIndex = (roadArtEditor->fieldIndex + 5) % 6;
    }
  }
  if (IsKeyPressed(KEY_DOWN)) {
    if (roadArtEditor->selectingAsset) {
      AdjustRoadArtEditorSprite(roadArtEditor, 1);
    } else {
      roadArtEditor->fieldIndex = (roadArtEditor->fieldIndex + 1) % 6;
    }
  }

  bool changedRoadArtTuning = false;
  if (IsKeyPressed(KEY_RIGHT)) {
    AdjustRoadArtEditorValue(roadArtTuning, *roadArtEditor, 1);
    changedRoadArtTuning = true;
  }
  if (IsKeyPressed(KEY_LEFT)) {
    AdjustRoadArtEditorValue(roadArtTuning, *roadArtEditor, -1);
    changedRoadArtTuning = true;
  }
  if (changedRoadArtTuning) {
    SaveRoadArtTuningWithLog(roadArtTuningPath, *roadArtTuning);
  }
}

void HandleGameHotkeys(bool pauseMenuActive, AppMode appMode, GameState* game,
                       RoadArtEditorState* roadArtEditor, RoadArtTuning* roadArtTuning,
                       const std::string& roadArtTuningPath, float* cameraZoomScale) {
  if (pauseMenuActive || appMode != AppMode::kGame) {
    return;
  }

  if (IsKeyPressed(KEY_A)) {
    ToggleDriveMode(game);
  }
  if (IsKeyPressed(KEY_E)) {
    roadArtEditor->active = !roadArtEditor->active;
    if (!roadArtEditor->active) {
      SaveRoadArtTuningWithLog(roadArtTuningPath, *roadArtTuning);
    }
  }
  if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
    *cameraZoomScale = std::max(kCameraZoomMin, *cameraZoomScale / kCameraZoomStep);
  }
  if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
    *cameraZoomScale = std::min(kCameraZoomMax, *cameraZoomScale * kCameraZoomStep);
  }
  if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0)) {
    *cameraZoomScale = 1.0f;
  }

  HandleRoadArtEditorInput(roadArtEditor, roadArtTuning, roadArtTuningPath);
}

void HandleHardwareTestHotkeys(bool pauseMenuActive, AppMode appMode, DisplayAxis* displayAxis) {
  if (pauseMenuActive || appMode != AppMode::kHardwareTest) {
    return;
  }

  if (IsKeyPressed(KEY_R)) {
    *displayAxis = DisplayAxis::kRoll;
  }
  if (IsKeyPressed(KEY_P)) {
    *displayAxis = DisplayAxis::kPitch;
  }
  if (IsKeyPressed(KEY_Y)) {
    *displayAxis = DisplayAxis::kYaw;
  }
}

}  // namespace

int main() {
  platform::UdpReceiver receiver;
  platform::UdpBroadcaster broadcaster;
  const bool udpReady = receiver.Open(kUdpPort);
  const bool broadcastReady = broadcaster.Open();
  const std::string localIpText = JoinLocalIps(platform::GetLocalIpv4Addresses());

  SetConfigFlags(platform::GetWindowConfigFlags());
  InitWindow(platform::kWindowWidth, platform::kWindowHeight, platform::kWindowTitle);
  platform::ApplyPostWindowInit();
  SetTargetFPS(60);
  InitAudioDevice();

  AppRuntime app;
  app.gameAssets = LoadGameAssets();
  std::vector<std::string> cityWarnings;
  app.city = LoadCityMap(game_asset_paths::FindCityPath("demo_city.csv"), &cityWarnings);
  PrintStartupWarnings("city", cityWarnings);
  app.roadArtTuningPath = game_asset_paths::FindConfigPath("road_art_tuning.csv");
  std::vector<std::string> roadArtWarnings;
  app.roadArtTuning = LoadRoadArtTuning(app.roadArtTuningPath, &roadArtWarnings);
  PrintStartupWarnings("road-art", roadArtWarnings);
  app.gameAudio = LoadGameAudio();
  InitializeGameFromCity(&app.game, &app.city);
  OpenPauseMenu(&app.pauseMenu);

  while (!app.shouldQuit && !WindowShouldClose()) {
    const auto frameStartTime = PerfClock::now();
    auto inputStartTime = frameStartTime;
    if (platform::ShouldToggleFullscreen()) {
      ToggleFullscreen();
    }
    if (!app.pauseMenu.active && IsKeyPressed(KEY_T)) {
      app.appMode = app.appMode == AppMode::kGame ? AppMode::kHardwareTest : AppMode::kGame;
    }
    HandleGameHotkeys(app.pauseMenu.active, app.appMode, &app.game, &app.roadArtEditor,
                      &app.roadArtTuning, app.roadArtTuningPath, &app.cameraZoomScale);
    HandleHardwareTestHotkeys(app.pauseMenu.active, app.appMode, &app.displayAxis);

    if (PollLatestSensorFrame(&receiver, &app.latestFrame)) {
      app.lastGoodFrame = app.latestFrame;
      app.lastPacketTime = std::chrono::steady_clock::now();
      MarkControllerPacketReceived(&app.discovery);
      RecordSensorFrameForSteering(app.lastGoodFrame, &app.steeringInput);
    }

    const float dt = GetFrameTime();
    const auto inputEndTime = PerfClock::now();
    const auto now = std::chrono::steady_clock::now();
    const float secondsSincePacket = std::chrono::duration<float>(now - app.lastPacketTime).count();
    const bool hasFreshPackets = secondsSincePacket <= kPacketTimeoutSeconds;
    const bool hasAnyPacket = app.lastPacketTime != std::chrono::steady_clock::time_point{};
    const bool roadArtEditorConsumesArrows =
        app.appMode == AppMode::kGame && app.roadArtEditor.active;
    const float rawKeyboardDirection =
        roadArtEditorConsumesArrows
            ? 0.0f
            : static_cast<float>(IsKeyDown(KEY_LEFT)) - static_cast<float>(IsKeyDown(KEY_RIGHT));
    const float keyboardTravelDirection = app.game.carSpeed < 0.0f ? -1.0f : 1.0f;
    const float keyboardDirection = rawKeyboardDirection * keyboardTravelDirection;
    UpdateKeyboardSteeringFallback(&app.steeringInput, keyboardDirection, hasFreshPackets, dt);

    UpdateServerDiscoveryBeacon(&app.discovery, &broadcaster, broadcastReady, hasFreshPackets,
                                app.lastPacketTime, now);

    const ControllerButtonState menuButtons =
        ReadControllerButtons(app.lastGoodFrame, hasFreshPackets);
    const bool pauseChordDown = menuButtons.green && menuButtons.red;
    const float pauseMenuSteeringAngleDeg =
        hasAnyPacket ? GetWrappedGameWheelAngleDeg(app.lastGoodFrame, app.steeringInput) *
                           kGameSteeringDirection
                     : app.steeringInput.manualAngleDeg;
    const PauseMenuAction pauseMenuAction =
        UpdatePauseMenu(&app.pauseMenu, pauseMenuSteeringAngleDeg, menuButtons.green,
                        menuButtons.red, dt);

    switch (pauseMenuAction) {
      case PauseMenuAction::kResume:
        ClosePauseMenu(&app.pauseMenu);
        break;
      case PauseMenuAction::kRestart:
        app.game = GameState{};
        InitializeGameFromCity(&app.game, &app.city);
        ClosePauseMenu(&app.pauseMenu);
        break;
      case PauseMenuAction::kCenter:
        ResetControllerCenter(app.lastGoodFrame, hasAnyPacket, &app.steeringInput);
        break;
      case PauseMenuAction::kQuit:
        app.shouldQuit = true;
        break;
      case PauseMenuAction::kToggleDriveMode:
        ToggleDriveMode(&app.game);
        break;
      case PauseMenuAction::kToggleHardwareTest:
        app.appMode = app.appMode == AppMode::kGame ? AppMode::kHardwareTest : AppMode::kGame;
        ClosePauseMenu(&app.pauseMenu);
        break;
      case PauseMenuAction::kZoomIn:
        app.cameraZoomScale =
            std::min(kCameraZoomMax, app.cameraZoomScale * kCameraZoomStep);
        break;
      case PauseMenuAction::kZoomOut:
        app.cameraZoomScale =
            std::max(kCameraZoomMin, app.cameraZoomScale / kCameraZoomStep);
        break;
      case PauseMenuAction::kNone:
        break;
    }

    if (UpdateRecenterGesture(&app.recenterGesture, menuButtons, app.pauseMenu.active,
                              pauseChordDown, now)) {
      ResetControllerCenter(app.lastGoodFrame, true, &app.steeringInput);
    }

    if (IsKeyPressed(KEY_SPACE)) {
      ResetControllerCenter(app.lastGoodFrame, hasAnyPacket, &app.steeringInput);
      ResetRecenterGesture(&app.recenterGesture);
    }

    const float sourceAngleDeg =
        hasAnyPacket ? GetAxisDegrees(app.displayAxis, app.lastGoodFrame)
                     : app.steeringInput.manualAngleDeg;
    const float centeredAngleDeg =
        hasAnyPacket && app.steeringInput.hasCenterFrame
            ? GetCenteredAxisDegrees(app.displayAxis, app.lastGoodFrame,
                                     app.steeringInput.centerFrame)
            : sourceAngleDeg;
    const float steeringAngleDeg = centeredAngleDeg * kDisplaySteeringDirection;
    const float normalizedValue = ClampUnit(steeringAngleDeg / kVisibleAngleRangeDeg);

    const float gameSourceAngleDeg =
        hasAnyPacket ? app.steeringInput.accumulatedGameAngleDeg
                     : app.steeringInput.manualAngleDeg;
    const float gameSteeringInput =
        ClampUnit(ApplyGameSteeringResponseCurve(gameSourceAngleDeg * kGameSteeringDirection));
    const ControllerButtonState gameInputButtons =
        ReadControllerButtons(app.lastGoodFrame, hasFreshPackets, !roadArtEditorConsumesArrows);
    const GameButtons gameButtons = {
        gameInputButtons.green,
        gameInputButtons.red,
    };
    const auto updateStartTime = PerfClock::now();
    if (app.appMode == AppMode::kGame && !app.pauseMenu.active) {
      UpdateGame(&app.game, gameSteeringInput, gameButtons, dt, &app.city);
    }
    const auto updateEndTime = PerfClock::now();
    const auto audioStartTime = updateEndTime;
    UpdateGameAudio(&app.gameAudio, app.game,
                    app.appMode == AppMode::kGame && !app.pauseMenu.active);
    const auto audioEndTime = PerfClock::now();

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    const auto drawStartTime = PerfClock::now();
    BeginDrawing();
    if (app.appMode == AppMode::kGame) {
      DrawGame(app.game, app.gameAssets, app.city, app.roadArtTuning, app.roadArtEditor,
               hasFreshPackets, hasAnyPacket, app.discovery.wasBroadcastingForLoss, screenWidth,
               screenHeight, app.cameraZoomScale);
      if (app.pauseMenu.active) {
        DrawPauseMenu(app.pauseMenu, app.game, app.appMode, localIpText, app.cameraZoomScale,
                      screenWidth, screenHeight);
      }
      EndDrawing();
      const auto frameEndTime = PerfClock::now();
      AddPerformanceSample(
          &app.performance, ElapsedMilliseconds(frameStartTime, frameEndTime),
          static_cast<double>(dt) * 1000.0, ElapsedMilliseconds(inputStartTime, inputEndTime),
          ElapsedMilliseconds(updateStartTime, updateEndTime),
          ElapsedMilliseconds(audioStartTime, audioEndTime),
          ElapsedMilliseconds(drawStartTime, frameEndTime));
      MaybeLogPerformance(&app.performance, app.appMode, app.pauseMenu.active);
      continue;
    }

    DrawHardwareTest(app.lastGoodFrame, app.displayAxis, sourceAngleDeg, centeredAngleDeg,
                     steeringAngleDeg, normalizedValue, menuButtons, hasFreshPackets,
                     hasAnyPacket, udpReady, localIpText, screenWidth, screenHeight);
    if (app.pauseMenu.active) {
      DrawPauseMenu(app.pauseMenu, app.game, app.appMode, localIpText, app.cameraZoomScale,
                    screenWidth, screenHeight);
    }

    EndDrawing();
    const auto frameEndTime = PerfClock::now();
    AddPerformanceSample(
        &app.performance, ElapsedMilliseconds(frameStartTime, frameEndTime),
        static_cast<double>(dt) * 1000.0, ElapsedMilliseconds(inputStartTime, inputEndTime),
        ElapsedMilliseconds(updateStartTime, updateEndTime),
        ElapsedMilliseconds(audioStartTime, audioEndTime),
        ElapsedMilliseconds(drawStartTime, frameEndTime));
    MaybeLogPerformance(&app.performance, app.appMode, app.pauseMenu.active);
  }

  UnloadGameAudio(&app.gameAudio);
  UnloadGameAssets(&app.gameAssets);
  if (IsAudioDeviceReady()) {
    CloseAudioDevice();
  }
  CloseWindow();
  return 0;
}
