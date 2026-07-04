#pragma once

#include <algorithm>

#include "../app/center_confirm.h"
#include "raylib.h"
#include "steering_wheel_2d.h"

inline void DrawCenterConfirm(const CenterConfirmState& state, float steeringAngleDeg,
                              bool hasFreshPackets, int screenWidth, int screenHeight) {
  if (!state.active) {
    return;
  }

  DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 130});

  const float panelWidth = std::min(680.0f, static_cast<float>(screenWidth) - 80.0f);
  const float panelHeight = std::min(620.0f, static_cast<float>(screenHeight) - 70.0f);
  const float panelX = (static_cast<float>(screenWidth) - panelWidth) * 0.5f;
  const float panelY = (static_cast<float>(screenHeight) - panelHeight) * 0.5f;
  const Rectangle panel = {panelX, panelY, panelWidth, panelHeight};

  DrawRectangleRounded(panel, 0.08f, 16, Color{73, 49, 31, 230});
  DrawRectangleRounded(Rectangle{panel.x + 3.0f, panel.y + 3.0f, panel.width - 6.0f,
                                 panel.height - 6.0f},
                       0.08f, 16, Color{252, 244, 222, 244});

  DrawText("Center Wheel", static_cast<int>(panelX + 34.0f),
           static_cast<int>(panelY + 28.0f), 40, Color{54, 36, 23, 255});
  DrawText("Press green when centered", static_cast<int>(panelX + 36.0f),
           static_cast<int>(panelY + 82.0f), 24, Color{92, 69, 49, 235});
  DrawText("Red / B backs out", static_cast<int>(panelX + 36.0f),
           static_cast<int>(panelY + 116.0f), 20, Color{92, 69, 49, 205});

  const Vector2 wheelCenter = {panelX + panelWidth * 0.5f, panelY + panelHeight * 0.56f};
  const float wheelRadius = std::clamp(std::min(panelWidth, panelHeight) * 0.25f, 120.0f,
                                       190.0f);
  steering_wheel_2d::DrawSteeringWheel(wheelCenter, wheelRadius, steeringAngleDeg);

  const Color statusColor =
      hasFreshPackets ? Color{59, 120, 87, 255} : Color{184, 72, 49, 255};
  const char* statusText = hasFreshPackets ? "Controller stream active" : "Keyboard fallback";
  const int statusWidth = MeasureText(statusText, 19);
  DrawText(statusText, static_cast<int>(wheelCenter.x - statusWidth * 0.5f),
           static_cast<int>(panelY + panelHeight - 96.0f), 19, statusColor);

  const char* angleText = TextFormat("wheel: %.1f deg", steeringAngleDeg);
  const int angleWidth = MeasureText(angleText, 22);
  DrawText(angleText, static_cast<int>(wheelCenter.x - angleWidth * 0.5f),
           static_cast<int>(panelY + panelHeight - 62.0f), 22, Color{54, 36, 23, 235});
}
