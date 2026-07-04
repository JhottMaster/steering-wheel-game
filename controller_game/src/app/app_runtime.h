#pragma once

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "app_mode.h"
#include "center_confirm.h"
#include "pause_menu.h"
#include "performance_log.h"
#include "server_discovery.h"
#include "../game/asset_paths.h"
#include "../game/city_map.h"
#include "../game/game_assets.h"
#include "../game/game_audio.h"
#include "../game/game_logic.h"
#include "../game/road_art_tuning.h"
#include "../input/recenter_gesture.h"
#include "../input/steering_input.h"
#include "../views/steering_wheel_3d.h"

struct AppRuntime {
  GameAssets gameAssets;
  SteeringWheel3DModel steeringWheel3D;
  CityMap city;
  std::string roadArtTuningPath;
  RoadArtTuning roadArtTuning;
  RoadArtEditorState roadArtEditor;
  GameAudio gameAudio;
  GameState game;
  SensorFrame latestFrame;
  SensorFrame lastGoodFrame;
  std::chrono::steady_clock::time_point lastPacketTime = {};
  OrientationAxis displayAxis = OrientationAxis::kPitch;
  AppMode appMode = AppMode::kGame;
  SteeringInputState steeringInput;
  DiscoveryBeaconState discovery;
  RecenterGestureState recenterGesture;
  float cameraZoomScale = 1.0f;
  PauseMenuState pauseMenu;
  CenterConfirmState centerConfirm;
  bool shouldQuit = false;
  PerformanceWindow performance;
};

inline void PrintStartupWarnings(const char* label, const std::vector<std::string>& warnings) {
  for (const std::string& warning : warnings) {
    std::printf("[%s] %s\n", label, warning.c_str());
  }
}

inline AppRuntime LoadAppRuntime() {
  AppRuntime app;
  app.gameAssets = LoadGameAssets();
  app.steeringWheel3D = steering_wheel_3d::LoadSteeringWheel3DModel();

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
  return app;
}

inline void UnloadAppRuntime(AppRuntime* app) {
  UnloadGameAudio(&app->gameAudio);
  steering_wheel_3d::UnloadSteeringWheel3DModel(&app->steeringWheel3D);
  UnloadGameAssets(&app->gameAssets);
}
