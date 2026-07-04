#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#if defined(_WIN32)
#include "../platform/platform_windows.h"
#else
#include "../platform/platform_linux.h"
#endif

#include "../app/controller_buttons.h"
#include "../game/game_logic.h"
#include "raylib.h"
#include "steering_wheel_3d.h"

using DisplayAxis = OrientationAxis;

namespace hardware_test_view_detail {
constexpr int kUdpPort = 4210;

inline const char* GetAxisLabel(DisplayAxis axis) {
  switch (axis) {
    case DisplayAxis::kRoll:
      return "roll axis";
    case DisplayAxis::kPitch:
      return "pitch axis";
    case DisplayAxis::kYaw:
      return "yaw axis";
  }

  return "pitch axis";
}

inline void DrawButtonLamp(Vector2 center, float radius, bool pressed, Color dimColor,
                           Color litColor, const char* label) {
  const Color lampColor = pressed ? litColor : dimColor;
  DrawCircleV(Vector2{center.x + 4.0f, center.y + 5.0f}, radius + 4.0f,
              Color{38, 46, 49, 85});
  DrawCircleV(center, radius + 5.0f, Color{46, 72, 88, 255});
  DrawCircleV(center, radius, lampColor);
  if (pressed) {
    DrawCircleV(Vector2{center.x - radius * 0.25f, center.y - radius * 0.25f},
                radius * 0.30f, Color{255, 255, 255, 95});
  }

  const int labelWidth = MeasureText(label, 20);
  DrawText(label, static_cast<int>(center.x - labelWidth / 2),
           static_cast<int>(center.y + radius + 12.0f), 20, Color{46, 72, 88, 255});
}

inline void DrawPanel(Rectangle bounds, Color fillColor) {
  DrawRectangleRounded(bounds, 0.10f, 10, fillColor);
  platform::DrawRoundedRectangleLines(bounds, 0.10f, 10, 2.0f, Color{206, 198, 179, 255});
}
}  // namespace hardware_test_view_detail

inline void DrawHardwareTest(const SensorFrame& lastGoodFrame, DisplayAxis displayAxis,
                             float sourceAngleDeg, float centeredAngleDeg,
                             float steeringAngleDeg, float normalizedValue,
                             const ControllerButtonState& buttons, bool hasFreshPackets,
                             bool hasAnyPacket, bool udpReady, const std::string& localIpText,
                             const SteeringWheel3DModel& wheelModel, int screenWidth,
                             int screenHeight) {
  ClearBackground(Color{242, 239, 228, 255});

  const float contentWidth = std::min(static_cast<float>(screenWidth) - 80.0f, 1540.0f);
  const float contentLeft = (static_cast<float>(screenWidth) - contentWidth) * 0.5f;
  const float contentRight = contentLeft + contentWidth;
  const float top = 26.0f;
  const float panelTop = 112.0f;
  const float bottomMargin = 32.0f;
  const float panelHeight =
      std::max(420.0f, static_cast<float>(screenHeight) - panelTop - bottomMargin);
  const float sidePanelWidth = std::clamp(contentWidth * 0.26f, 320.0f, 430.0f);
  const float gap = 24.0f;
  const Rectangle leftPanel = {contentLeft, panelTop, sidePanelWidth, panelHeight};
  const Rectangle rightPanel = {contentRight - sidePanelWidth, panelTop, sidePanelWidth,
                                panelHeight};
  const Rectangle wheelPanel = {leftPanel.x + leftPanel.width + gap, panelTop,
                                contentWidth - sidePanelWidth * 2.0f - gap * 2.0f,
                                panelHeight};

  DrawText(platform::kHeaderTitle, static_cast<int>(contentLeft), static_cast<int>(top), 34,
           Color{46, 72, 88, 255});
  DrawText(platform::kHeaderHelp, static_cast<int>(contentLeft), static_cast<int>(top + 42.0f),
           21, Color{77, 92, 103, 255});

  hardware_test_view_detail::DrawPanel(leftPanel, Color{255, 250, 235, 235});
  hardware_test_view_detail::DrawPanel(rightPanel, Color{255, 250, 235, 235});
  hardware_test_view_detail::DrawPanel(wheelPanel, Color{248, 244, 231, 235});

  const Vector2 center = {wheelPanel.x + wheelPanel.width * 0.5f,
                          wheelPanel.y + wheelPanel.height * 0.52f};
  const float wheelRadius = std::clamp(std::min(wheelPanel.width, wheelPanel.height) * 0.32f,
                                       135.0f, 270.0f);
  steering_wheel_3d::DrawSteeringWheel3D(wheelModel, lastGoodFrame.orientation, hasAnyPacket,
                                         steeringAngleDeg, wheelPanel);
  hardware_test_view_detail::DrawButtonLamp(Vector2{center.x - wheelRadius * 1.34f, center.y},
                                            wheelRadius * 0.16f, buttons.red,
                                            Color{92, 35, 34, 255},
                                            Color{237, 54, 43, 255}, "button 2");
  hardware_test_view_detail::DrawButtonLamp(Vector2{center.x + wheelRadius * 1.34f, center.y},
                                            wheelRadius * 0.16f, buttons.green,
                                            Color{35, 84, 50, 255},
                                            Color{52, 222, 98, 255}, "button 1");

  const int leftX = static_cast<int>(leftPanel.x + 24.0f);
  int y = static_cast<int>(leftPanel.y + 24.0f);
  DrawText("Steering Input", leftX, y, 26, Color{46, 72, 88, 255});
  y += 44;
  DrawText(TextFormat("axis: %s", hardware_test_view_detail::GetAxisLabel(displayAxis)), leftX, y,
           23, Color{46, 72, 88, 255});
  y += 38;
  DrawText(TextFormat("axis twist: %.1f deg", sourceAngleDeg), leftX, y, 23,
           Color{46, 72, 88, 255});
  y += 38;
  DrawText(TextFormat("centered: %.1f deg", centeredAngleDeg), leftX, y, 23,
           Color{46, 72, 88, 255});
  y += 38;
  DrawText(TextFormat("wheel: %.1f deg", steeringAngleDeg), leftX, y, 23,
           Color{46, 72, 88, 255});
  y += 38;
  DrawText(TextFormat("normalized: %.2f", normalizedValue), leftX, y, 23,
           Color{46, 72, 88, 255});
  y += 56;
  DrawText("Buttons", leftX, y, 26, Color{46, 72, 88, 255});
  y += 44;
  DrawText(TextFormat("green/right: %s", buttons.green ? "pressed" : "up"), leftX,
           y, 22, Color{46, 72, 88, 255});
  y += 34;
  DrawText(TextFormat("red/left: %s", buttons.red ? "pressed" : "up"), leftX, y,
           22, Color{46, 72, 88, 255});

  const int rightX = static_cast<int>(rightPanel.x + 24.0f);
  y = static_cast<int>(rightPanel.y + 24.0f);
  const char* inputMode = hasFreshPackets
                              ? "UDP sensor stream active"
                              : (hasAnyPacket ? "Showing stale packet" : "Keyboard fallback");
  DrawText("Connection", rightX, y, 26, Color{46, 72, 88, 255});
  y += 44;
  DrawText(inputMode, rightX, y, 21,
           hasFreshPackets ? Color{59, 120, 87, 255}
                           : (hasAnyPacket ? Color{191, 134, 33, 255}
                                           : Color{184, 72, 49, 255}));
  y += 38;
  DrawText(TextFormat("UDP port: %d", hardware_test_view_detail::kUdpPort), rightX, y, 21,
           Color{77, 92, 103, 255});
  y += 34;
  DrawText(udpReady ? "Listener: ready" : "Listener: failed", rightX, y, 21,
           udpReady ? Color{59, 120, 87, 255} : Color{184, 72, 49, 255});
  y += 34;
  DrawText(TextFormat("FPS: %d", GetFPS()), rightX, y, 21, Color{46, 72, 88, 255});
  y += 46;
  DrawText("Host IPv4", rightX, y, 22, Color{77, 92, 103, 255});
  y += 30;
  DrawText(localIpText.c_str(), rightX, y, 20, Color{46, 72, 88, 255});
  y += 56;
  DrawText("Latest packet", rightX, y, 22, Color{77, 92, 103, 255});
  y += 32;
  DrawText(TextFormat("quat w %.3f", lastGoodFrame.orientation.w), rightX, y, 20,
           Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("quat x %.3f", lastGoodFrame.orientation.x), rightX, y, 20,
           Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("quat y %.3f", lastGoodFrame.orientation.y), rightX, y, 20,
           Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("quat z %.3f", lastGoodFrame.orientation.z), rightX, y, 20,
           Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("euler roll %.1f", QuaternionToRollDegrees(lastGoodFrame.orientation)),
           rightX, y, 20, Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("euler pitch %.1f", QuaternionToPitchDegrees(lastGoodFrame.orientation)),
           rightX, y, 20, Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("euler yaw %.1f", QuaternionToYawDegrees(lastGoodFrame.orientation)),
           rightX, y, 20, Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("button1 %s", buttons.green ? "pressed" : "up"), rightX, y,
           20, Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("button2 %s", buttons.red ? "pressed" : "up"), rightX, y,
           20, Color{46, 72, 88, 255});
}
