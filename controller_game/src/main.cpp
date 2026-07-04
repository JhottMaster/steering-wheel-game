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
#include "game/game_audio.h"
#include "game/game_logic.h"
#include "game/game_view.h"
#include "game/road_art_tuning.h"
#include "input/sensor_receiver.h"
#include "raylib.h"
#include "views/hardware_test_view.h"
#include "views/pause_menu_view.h"

namespace {
constexpr int kUdpPort = 4210;
constexpr int kServerDiscoveryPort = 4211;
constexpr float kServerBeaconIntervalSeconds = 3.0f;
constexpr float kServerBeaconStartDelaySeconds = 5.0f;
constexpr float kVisibleAngleRangeDeg = 45.0f;
constexpr float kGameSteeringDeadzoneDeg = 2.0f;
constexpr float kGameSteeringDeadzoneBlendDeg = 8.0f;
constexpr float kGameSteeringEarlyResponseFraction = 0.85f;
constexpr float kGameSteeringEarlyRangeDeg = 90.0f;
constexpr float kGameSteeringFullLockDeg = 270.0f;
constexpr float kKeyboardSteeringResponsePerSecond = 220.0f;
constexpr float kKeyboardSteeringReturnPerSecond = 280.0f;
constexpr float kPacketTimeoutSeconds = 1.0f;
constexpr float kDisplaySteeringDirection = -1.0f;
constexpr float kGameSteeringDirection = -1.0f;
constexpr float kCameraZoomMin = 0.15f;
constexpr float kCameraZoomMax = 1.5f;
constexpr float kCameraZoomStep = 1.25f;
constexpr float kRecenterGestureWindowSeconds = 1.5f;
constexpr int kRecenterRedPressesRequired = 3;
constexpr char kServerDiscoveryMessage[] = "steering-wheel-server port=4210";
constexpr double kPerformanceLogIntervalSeconds = 2.0;

using PerfClock = std::chrono::steady_clock;

struct PerfBucket {
  double totalMs = 0.0;
  double maxMs = 0.0;

  void Add(double milliseconds) {
    totalMs += milliseconds;
    maxMs = std::max(maxMs, milliseconds);
  }

  double Average(int sampleCount) const {
    return sampleCount > 0 ? totalMs / static_cast<double>(sampleCount) : 0.0;
  }
};

struct PerformanceWindow {
  PerfBucket frameCpu;
  PerfBucket dt;
  PerfBucket input;
  PerfBucket update;
  PerfBucket audio;
  PerfBucket draw;
  int frames = 0;
  PerfClock::time_point startedAt = PerfClock::now();
};

double ElapsedMilliseconds(PerfClock::time_point start, PerfClock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

void AddPerformanceSample(PerformanceWindow* performance, double frameCpuMs, double dtMs,
                          double inputMs, double updateMs, double audioMs, double drawMs) {
  performance->frameCpu.Add(frameCpuMs);
  performance->dt.Add(dtMs);
  performance->input.Add(inputMs);
  performance->update.Add(updateMs);
  performance->audio.Add(audioMs);
  performance->draw.Add(drawMs);
  ++performance->frames;
}

void MaybeLogPerformance(PerformanceWindow* performance, AppMode appMode, bool pauseMenuActive) {
  const auto now = PerfClock::now();
  const double elapsedSeconds =
      std::chrono::duration<double>(now - performance->startedAt).count();
  if (elapsedSeconds < kPerformanceLogIntervalSeconds || performance->frames <= 0) {
    return;
  }

  const double averageFrameCpuMs = performance->frameCpu.Average(performance->frames);
  const double averageDtMs = performance->dt.Average(performance->frames);
  const double averageInputMs = performance->input.Average(performance->frames);
  const double averageUpdateMs = performance->update.Average(performance->frames);
  const double averageAudioMs = performance->audio.Average(performance->frames);
  const double averageDrawMs = performance->draw.Average(performance->frames);
  const double averageOtherMs =
      std::max(0.0, averageFrameCpuMs -
                        (averageInputMs + averageUpdateMs + averageAudioMs + averageDrawMs));

  std::printf(
      "[perf] mode=%s pause=%s frames=%d dt(avg/max)=%.2f/%.2f ms cpu(avg/max)=%.2f/%.2f ms "
      "input=%.2f update=%.2f audio=%.2f draw=%.2f other=%.2f\n",
      appMode == AppMode::kGame ? "game" : "hardware", pauseMenuActive ? "yes" : "no",
      performance->frames, averageDtMs, performance->dt.maxMs, averageFrameCpuMs,
      performance->frameCpu.maxMs, averageInputMs, averageUpdateMs, averageAudioMs,
      averageDrawMs, averageOtherMs);

  *performance = PerformanceWindow{};
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

float GetWrappedGameWheelAngleDeg(const SensorFrame& frame, const SensorFrame& centerFrame,
                                  bool hasCenterFrame) {
  return hasCenterFrame ? GetCenteredAxisDegrees(DisplayAxis::kPitch, frame, centerFrame)
                        : GetAxisDegrees(DisplayAxis::kPitch, frame);
}

float ApplyGameSteeringResponseCurve(float wheelAngleDeg) {
  const float sign = wheelAngleDeg < 0.0f ? -1.0f : 1.0f;
  const float absoluteAngleDeg = std::fabs(wheelAngleDeg);
  if (absoluteAngleDeg <= kGameSteeringDeadzoneDeg) {
    return 0.0f;
  }

  const float deadzoneBlendEndDeg = kGameSteeringDeadzoneDeg + kGameSteeringDeadzoneBlendDeg;
  float softenedAngleDeg = absoluteAngleDeg;
  if (absoluteAngleDeg < deadzoneBlendEndDeg) {
    const float t =
        (absoluteAngleDeg - kGameSteeringDeadzoneDeg) / kGameSteeringDeadzoneBlendDeg;
    const float smoothedT = t * t * (3.0f - 2.0f * t);
    softenedAngleDeg = smoothedT * deadzoneBlendEndDeg;
  }

  const float clampedAngleDeg = std::min(softenedAngleDeg, kGameSteeringFullLockDeg);

  if (clampedAngleDeg <= kGameSteeringEarlyRangeDeg) {
    const float earlyFraction = clampedAngleDeg / kGameSteeringEarlyRangeDeg;
    return sign * earlyFraction * kGameSteeringEarlyResponseFraction;
  }

  const float remainingAngleDeg = kGameSteeringFullLockDeg - kGameSteeringEarlyRangeDeg;
  const float trailingFraction =
      remainingAngleDeg <= 0.0f ? 1.0f
                                : (clampedAngleDeg - kGameSteeringEarlyRangeDeg) / remainingAngleDeg;
  const float normalized =
      kGameSteeringEarlyResponseFraction +
      trailingFraction * (1.0f - kGameSteeringEarlyResponseFraction);
  return sign * normalized;
}

void ResetControllerCenter(const SensorFrame& lastGoodFrame, bool hasAnyPacket, SensorFrame* centerFrame,
                           bool* hasCenterFrame, float* manualAngleDeg,
                           float* gameAccumulatedAngleDeg, float* lastWrappedGameAngleDeg,
                           bool* hasLastWrappedGameAngle) {
  if (hasAnyPacket) {
    *centerFrame = lastGoodFrame;
    *hasCenterFrame = true;
  } else {
    *manualAngleDeg = 0.0f;
    *hasCenterFrame = false;
  }
  *gameAccumulatedAngleDeg = 0.0f;
  *lastWrappedGameAngleDeg = 0.0f;
  *hasLastWrappedGameAngle = false;
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
  CityMap city = LoadCityMap(game_assets_detail::FindCityPath("demo_city.csv"));
  const std::string roadArtTuningPath = game_assets_detail::FindConfigPath("road_art_tuning.csv");
  RoadArtTuning roadArtTuning = LoadRoadArtTuning(roadArtTuningPath);
  RoadArtEditorState roadArtEditor;
  GameAudio gameAudio = LoadGameAudio();
  GameState game;
  InitializeGameFromCity(&game, &city);
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
        SaveRoadArtTuning(roadArtTuningPath, roadArtTuning);
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
        SaveRoadArtTuning(roadArtTuningPath, roadArtTuning);
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
    if (!hasFreshPackets) {
      const float targetManualAngleDeg = keyboardDirection * kVisibleAngleRangeDeg;
      const float responsePerSecond =
          keyboardDirection == 0.0f ? kKeyboardSteeringReturnPerSecond
                                    : kKeyboardSteeringResponsePerSecond;
      const float maxStepDeg = responsePerSecond * dt;
      if (manualAngleDeg < targetManualAngleDeg) {
        manualAngleDeg = std::min(manualAngleDeg + maxStepDeg, targetManualAngleDeg);
      } else if (manualAngleDeg > targetManualAngleDeg) {
        manualAngleDeg = std::max(manualAngleDeg - maxStepDeg, targetManualAngleDeg);
      }
    } else {
      manualAngleDeg = 0.0f;
    }

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

    const ControllerButtonState menuButtons =
        ReadControllerButtons(lastGoodFrame, hasFreshPackets);
    const bool pauseChordDown = menuButtons.green && menuButtons.red;
    const float pauseMenuSteeringAngleDeg =
        hasAnyPacket ? GetWrappedGameWheelAngleDeg(lastGoodFrame, centerFrame, hasCenterFrame) *
                           kGameSteeringDirection
                     : manualAngleDeg;
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
        ResetControllerCenter(lastGoodFrame, hasAnyPacket, &centerFrame, &hasCenterFrame,
                              &manualAngleDeg, &gameAccumulatedAngleDeg, &lastWrappedGameAngleDeg,
                              &hasLastWrappedGameAngle);
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

    if (pauseChordDown) {
      greenButtonGestureActive = false;
      greenButtonGestureStartTime = std::chrono::steady_clock::time_point{};
      redButtonPressCount = 0;
      redButtonWasDown = menuButtons.red;
    } else if (!pauseMenu.active && menuButtons.green && !greenButtonGestureActive) {
      greenButtonGestureActive = true;
      greenButtonGestureStartTime = now;
      redButtonPressCount = 0;
      redButtonWasDown = menuButtons.red;
      std::printf("Recenter gesture started. Hold green and press red %d times within %.1f seconds.\n",
                  kRecenterRedPressesRequired, kRecenterGestureWindowSeconds);
    }

    if (!pauseMenu.active && greenButtonGestureActive) {
      const float gestureElapsedSeconds =
          std::chrono::duration<float>(now - greenButtonGestureStartTime).count();

      if (menuButtons.red && !redButtonWasDown) {
        ++redButtonPressCount;
        std::printf("Recenter gesture red press %d/%d.\n", redButtonPressCount,
                    kRecenterRedPressesRequired);
      }
      redButtonWasDown = menuButtons.red;

      if (!menuButtons.green) {
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
      ResetControllerCenter(lastGoodFrame, hasAnyPacket, &centerFrame, &hasCenterFrame,
                            &manualAngleDeg, &gameAccumulatedAngleDeg, &lastWrappedGameAngleDeg,
                            &hasLastWrappedGameAngle);
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
               hasAnyPacket, wasBroadcastingForLoss, screenWidth, screenHeight, cameraZoomScale);
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
