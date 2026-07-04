#pragma once

#include <algorithm>

#include "app_runtime.h"
#include "frame_input.h"
#include "raylib.h"

constexpr float kCameraZoomMin = 0.15f;
constexpr float kCameraZoomMax = 1.5f;
constexpr float kCameraZoomStep = 1.25f;

inline void ApplyPauseMenuAction(AppRuntime* app, PauseMenuAction action,
                                 const FrameInput& input) {
  switch (action) {
    case PauseMenuAction::kResume:
      ClosePauseMenu(&app->pauseMenu);
      break;
    case PauseMenuAction::kRestart:
      app->game = GameState{};
      InitializeGameFromCity(&app->game, &app->city);
      ClosePauseMenu(&app->pauseMenu);
      break;
    case PauseMenuAction::kCenter:
      OpenCenterConfirm(&app->centerConfirm, input.menuButtons.green,
                        input.menuButtons.red);
      break;
    case PauseMenuAction::kQuit:
      app->shouldQuit = true;
      break;
    case PauseMenuAction::kToggleDriveMode:
      ToggleDriveMode(&app->game);
      break;
    case PauseMenuAction::kToggleHardwareTest:
      app->appMode = app->appMode == AppMode::kGame ? AppMode::kHardwareTest : AppMode::kGame;
      ClosePauseMenu(&app->pauseMenu);
      break;
    case PauseMenuAction::kZoomIn:
      app->cameraZoomScale =
          std::min(kCameraZoomMax, app->cameraZoomScale * kCameraZoomStep);
      break;
    case PauseMenuAction::kZoomOut:
      app->cameraZoomScale =
          std::max(kCameraZoomMin, app->cameraZoomScale / kCameraZoomStep);
      break;
    case PauseMenuAction::kNone:
      break;
  }
}

inline void UpdateMenusAndOverlays(AppRuntime* app, const FrameInput& input) {
  const bool centerConfirmWasActive = app->centerConfirm.active;
  const CenterConfirmAction centerConfirmAction =
      UpdateCenterConfirm(&app->centerConfirm, input.menuButtons.green,
                          input.menuButtons.red);
  if (centerConfirmAction == CenterConfirmAction::kConfirm) {
    ResetControllerCenter(app->lastGoodFrame, input.hasAnyPacket, &app->steeringInput);
    CloseCenterConfirm(&app->centerConfirm);
  } else if (centerConfirmAction == CenterConfirmAction::kCancel) {
    CloseCenterConfirm(&app->centerConfirm);
  }

  const PauseMenuAction pauseMenuAction =
      centerConfirmWasActive
          ? PauseMenuAction::kNone
          : UpdatePauseMenu(&app->pauseMenu, input.pauseMenuSteeringAngleDeg,
                            input.menuButtons.green, input.menuButtons.red, input.dt);
  ApplyPauseMenuAction(app, pauseMenuAction, input);

  if (UpdateRecenterGesture(&app->recenterGesture, input.menuButtons, app->pauseMenu.active,
                            input.pauseChordDown, input.now)) {
    ResetControllerCenter(app->lastGoodFrame, true, &app->steeringInput);
  }

  if (IsKeyPressed(KEY_SPACE)) {
    ResetControllerCenter(app->lastGoodFrame, input.hasAnyPacket, &app->steeringInput);
    ResetRecenterGesture(&app->recenterGesture);
  }
}
