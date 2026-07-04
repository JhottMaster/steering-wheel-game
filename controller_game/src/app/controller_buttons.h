#pragma once

#include "../game/game_logic.h"
#include "raylib.h"

struct ControllerButtonState {
  bool green = false;
  bool red = false;
};

inline ControllerButtonState ReadControllerButtons(const SensorFrame& frame, bool hasFreshPackets,
                                                   bool includeKeyboardArrows = true) {
  ControllerButtonState buttons = {
      hasFreshPackets && frame.button1Pressed,
      hasFreshPackets && frame.button2Pressed,
  };

  if (includeKeyboardArrows) {
    buttons.green = buttons.green || IsKeyDown(KEY_UP);
    buttons.red = buttons.red || IsKeyDown(KEY_DOWN);
  }

  return buttons;
}
