#pragma once

#include <chrono>
#include <cstdio>

#include "../app/controller_buttons.h"

constexpr float kRecenterGestureWindowSeconds = 1.5f;
constexpr int kRecenterRedPressesRequired = 3;

struct RecenterGestureState {
  bool active = false;
  std::chrono::steady_clock::time_point startedAt = {};
  bool redWasDown = false;
  int redPressCount = 0;
};

inline void ResetRecenterGesture(RecenterGestureState* gesture) {
  *gesture = RecenterGestureState{};
}

inline bool UpdateRecenterGesture(RecenterGestureState* gesture,
                                  const ControllerButtonState& buttons,
                                  bool pauseMenuActive, bool pauseChordDown,
                                  std::chrono::steady_clock::time_point now,
                                  bool logEvents = true) {
  if (pauseChordDown) {
    ResetRecenterGesture(gesture);
    gesture->redWasDown = buttons.red;
    return false;
  }

  if (!pauseMenuActive && buttons.green && !gesture->active) {
    gesture->active = true;
    gesture->startedAt = now;
    gesture->redPressCount = 0;
    gesture->redWasDown = buttons.red;
    if (logEvents) {
      std::printf(
          "Recenter gesture started. Hold green and press red %d times within %.1f seconds.\n",
          kRecenterRedPressesRequired, kRecenterGestureWindowSeconds);
    }
  }

  if (pauseMenuActive || !gesture->active) {
    return false;
  }

  const float gestureElapsedSeconds =
      std::chrono::duration<float>(now - gesture->startedAt).count();

  if (buttons.red && !gesture->redWasDown) {
    ++gesture->redPressCount;
    if (logEvents) {
      std::printf("Recenter gesture red press %d/%d.\n", gesture->redPressCount,
                  kRecenterRedPressesRequired);
    }
  }
  gesture->redWasDown = buttons.red;

  if (!buttons.green) {
    if (logEvents) {
      std::printf("Recenter gesture cancelled: green released.\n");
    }
    ResetRecenterGesture(gesture);
    return false;
  }

  if (gestureElapsedSeconds <= kRecenterGestureWindowSeconds) {
    return false;
  }

  const bool shouldResetCenter = gesture->redPressCount >= kRecenterRedPressesRequired;
  if (logEvents) {
    if (shouldResetCenter) {
      std::printf("Controller center reset from green hold + red triple press gesture.\n");
    } else {
      std::printf("Recenter gesture timed out at %.2f seconds with %d/%d red presses.\n",
                  gestureElapsedSeconds, gesture->redPressCount, kRecenterRedPressesRequired);
    }
  }
  ResetRecenterGesture(gesture);
  return shouldResetCenter;
}
