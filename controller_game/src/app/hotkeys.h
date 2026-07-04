#pragma once

#include <algorithm>
#include <cstdio>
#include <string>

#include "app_mode.h"
#include "menu_flow.h"
#include "../game/game_logic.h"
#include "../game/road_art_tuning.h"
#include "raylib.h"

inline void SaveRoadArtTuningWithLog(const std::string& path, const RoadArtTuning& tuning) {
  if (!SaveRoadArtTuning(path, tuning)) {
    std::printf("[road-art] failed to save tuning config: %s\n", path.c_str());
  }
}

inline void HandleRoadArtEditorInput(RoadArtEditorState* roadArtEditor,
                                     RoadArtTuning* roadArtTuning,
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

inline void HandleGameHotkeys(bool pauseMenuActive, AppMode appMode, GameState* game,
                              RoadArtEditorState* roadArtEditor,
                              RoadArtTuning* roadArtTuning,
                              const std::string& roadArtTuningPath,
                              float* cameraZoomScale) {
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

inline void HandleHardwareTestHotkeys(bool pauseMenuActive, AppMode appMode,
                                      OrientationAxis* displayAxis) {
  if (pauseMenuActive || appMode != AppMode::kHardwareTest) {
    return;
  }

  if (IsKeyPressed(KEY_R)) {
    *displayAxis = OrientationAxis::kRoll;
  }
  if (IsKeyPressed(KEY_P)) {
    *displayAxis = OrientationAxis::kPitch;
  }
  if (IsKeyPressed(KEY_Y)) {
    *displayAxis = OrientationAxis::kYaw;
  }
}
