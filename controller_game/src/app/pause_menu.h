#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "app_mode.h"
#include "../game/game_logic.h"

constexpr float kPauseChordHoldSeconds = 3.0f;
constexpr float kPauseMenuSteerThresholdDeg = 18.0f;
constexpr float kPauseMenuSteerResetDeg = 8.0f;

enum class PauseMenuAction {
  kNone,
  kResume,
  kRestart,
  kCenter,
  kQuit,
  kToggleDriveMode,
  kToggleHardwareTest,
  kZoomIn,
  kZoomOut,
};

enum class PauseMenuItem {
  kResume,
  kRestart,
  kCenter,
  kQuit,
  kToggleDriveMode,
  kHardwareTest,
  kZoomIn,
  kZoomOut,
};

struct PauseMenuState {
  bool active = false;
  int selectedIndex = 0;
  bool steerArmed = true;
  bool greenWasDown = false;
  bool redWasDown = false;
  float pauseChordSeconds = 0.0f;
};

inline constexpr std::array<PauseMenuItem, 8> kPauseMenuItems = {
    PauseMenuItem::kResume,
    PauseMenuItem::kRestart,
    PauseMenuItem::kCenter,
    PauseMenuItem::kQuit,
    PauseMenuItem::kToggleDriveMode,
    PauseMenuItem::kHardwareTest,
    PauseMenuItem::kZoomOut,
    PauseMenuItem::kZoomIn,
};

inline void OpenPauseMenu(PauseMenuState* menu) {
  menu->active = true;
  menu->selectedIndex = 0;
  menu->steerArmed = true;
  menu->greenWasDown = true;
  menu->redWasDown = true;
  menu->pauseChordSeconds = 0.0f;
}

inline void ClosePauseMenu(PauseMenuState* menu) {
  menu->active = false;
  menu->steerArmed = true;
  menu->pauseChordSeconds = 0.0f;
}

inline PauseMenuItem GetSelectedPauseMenuItem(const PauseMenuState& menu) {
  const int clampedIndex =
      std::clamp(menu.selectedIndex, 0, static_cast<int>(kPauseMenuItems.size()) - 1);
  return kPauseMenuItems[static_cast<size_t>(clampedIndex)];
}

inline const char* GetPauseMenuItemLabel(PauseMenuItem item, const GameState& game,
                                         AppMode appMode) {
  switch (item) {
    case PauseMenuItem::kResume:
      return "Resume";
    case PauseMenuItem::kRestart:
      return "Restart";
    case PauseMenuItem::kCenter:
      return "Center Wheel";
    case PauseMenuItem::kQuit:
      return "Quit";
    case PauseMenuItem::kToggleDriveMode:
      return game.driveMode == DriveMode::kManual ? "Mode: Button Gas" : "Mode: Cruise";
    case PauseMenuItem::kHardwareTest:
      return appMode == AppMode::kHardwareTest ? "Back to Game" : "Hardware Test";
    case PauseMenuItem::kZoomIn:
      return "Zoom In";
    case PauseMenuItem::kZoomOut:
      return "Zoom Out";
  }
  return "";
}

inline PauseMenuAction GetPauseMenuAction(PauseMenuItem item) {
  switch (item) {
    case PauseMenuItem::kResume:
      return PauseMenuAction::kResume;
    case PauseMenuItem::kRestart:
      return PauseMenuAction::kRestart;
    case PauseMenuItem::kCenter:
      return PauseMenuAction::kCenter;
    case PauseMenuItem::kQuit:
      return PauseMenuAction::kQuit;
    case PauseMenuItem::kToggleDriveMode:
      return PauseMenuAction::kToggleDriveMode;
    case PauseMenuItem::kHardwareTest:
      return PauseMenuAction::kToggleHardwareTest;
    case PauseMenuItem::kZoomIn:
      return PauseMenuAction::kZoomIn;
    case PauseMenuItem::kZoomOut:
      return PauseMenuAction::kZoomOut;
  }
  return PauseMenuAction::kNone;
}

inline PauseMenuAction UpdatePauseMenu(PauseMenuState* menu, float steeringAngleDeg,
                                       bool greenDown, bool redDown, float dt) {
  if (!menu->active) {
    if (greenDown && redDown) {
      menu->pauseChordSeconds += dt;
      if (menu->pauseChordSeconds >= kPauseChordHoldSeconds) {
        OpenPauseMenu(menu);
      }
    } else {
      menu->pauseChordSeconds = 0.0f;
    }
    menu->greenWasDown = greenDown;
    menu->redWasDown = redDown;
    return PauseMenuAction::kNone;
  }

  const bool greenPressed = greenDown && !menu->greenWasDown;
  const bool redPressed = redDown && !menu->redWasDown;

  if (std::fabs(steeringAngleDeg) <= kPauseMenuSteerResetDeg) {
    menu->steerArmed = true;
  }
  if (menu->steerArmed && steeringAngleDeg >= kPauseMenuSteerThresholdDeg) {
    menu->selectedIndex =
        (menu->selectedIndex + 1) % static_cast<int>(kPauseMenuItems.size());
    menu->steerArmed = false;
  } else if (menu->steerArmed && steeringAngleDeg <= -kPauseMenuSteerThresholdDeg) {
    menu->selectedIndex =
        (menu->selectedIndex + static_cast<int>(kPauseMenuItems.size()) - 1) %
        static_cast<int>(kPauseMenuItems.size());
    menu->steerArmed = false;
  }

  PauseMenuAction action = PauseMenuAction::kNone;
  if (redPressed) {
    action = PauseMenuAction::kResume;
  } else if (greenPressed) {
    action = GetPauseMenuAction(GetSelectedPauseMenuItem(*menu));
  }

  menu->greenWasDown = greenDown;
  menu->redWasDown = redDown;
  return action;
}
