#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#if defined(_WIN32)
#include "../platform/platform_windows.h"
#else
#include "../platform/platform_linux.h"
#endif

#include "../game/game_logic.h"
#include "raylib.h"

enum class DisplayAxis {
  kRoll,
  kPitch,
  kYaw,
};

namespace hardware_test_view_detail {
constexpr int kUdpPort = 4210;
constexpr float kDegToRad = 0.017453292519943295769f;

inline const char* GetAxisLabel(DisplayAxis axis) {
  switch (axis) {
    case DisplayAxis::kRoll:
      return "roll";
    case DisplayAxis::kPitch:
      return "pitch";
    case DisplayAxis::kYaw:
      return "yaw";
  }

  return "pitch";
}

inline Vector2 PointOnCircle(Vector2 center, float radius, float angleDeg) {
  const float radians = angleDeg * kDegToRad;
  return Vector2{center.x + std::cos(radians) * radius,
                 center.y + std::sin(radians) * radius};
}

inline void DrawSteeringWheel(Vector2 center, float radius, float rotationDeg) {
  const Color rimColor = Color{46, 72, 88, 255};
  const Color spokeColor = Color{77, 92, 103, 255};
  const Color accentColor = Color{184, 72, 49, 255};
  const Color shadowColor = Color{214, 209, 196, 255};
  const Color hubColor = Color{242, 239, 228, 255};

  DrawCircleV(Vector2{center.x + 5.0f, center.y + 7.0f}, radius + 6.0f, shadowColor);
  DrawRing(center, radius * 0.76f, radius, 0.0f, 360.0f, 96, rimColor);
  DrawRing(center, radius * 0.56f, radius * 0.62f, 0.0f, 360.0f, 96,
           Color{204, 211, 214, 255});

  for (int i = 0; i < 3; ++i) {
    const float spokeAngleDeg = rotationDeg - 90.0f + static_cast<float>(i) * 120.0f;
    DrawLineEx(PointOnCircle(center, radius * 0.20f, spokeAngleDeg),
               PointOnCircle(center, radius * 0.70f, spokeAngleDeg), radius * 0.09f,
               spokeColor);
  }

  DrawCircleV(center, radius * 0.24f, rimColor);
  DrawCircleV(center, radius * 0.14f, hubColor);
  DrawCircleV(PointOnCircle(center, radius * 0.87f, rotationDeg - 90.0f), radius * 0.075f,
              accentColor);
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

inline float GetAxisDegrees(DisplayAxis axis, const SensorFrame& frame) {
  switch (axis) {
    case DisplayAxis::kRoll:
      return frame.roll;
    case DisplayAxis::kPitch:
      return frame.pitch;
    case DisplayAxis::kYaw:
      return frame.heading;
  }

  return frame.pitch;
}

inline void DrawHardwareTest(const SensorFrame& lastGoodFrame, DisplayAxis displayAxis,
                             float sourceAngleDeg, float centeredAngleDeg,
                             float steeringAngleDeg, float normalizedValue, bool hasFreshPackets,
                             bool hasAnyPacket, bool udpReady, const std::string& localIpText,
                             int screenWidth, int screenHeight) {
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
  hardware_test_view_detail::DrawSteeringWheel(center, wheelRadius, steeringAngleDeg);
  hardware_test_view_detail::DrawButtonLamp(Vector2{center.x - wheelRadius * 1.34f, center.y},
                                            wheelRadius * 0.16f, lastGoodFrame.button2Pressed,
                                            Color{92, 35, 34, 255},
                                            Color{237, 54, 43, 255}, "button 2");
  hardware_test_view_detail::DrawButtonLamp(Vector2{center.x + wheelRadius * 1.34f, center.y},
                                            wheelRadius * 0.16f, lastGoodFrame.button1Pressed,
                                            Color{35, 84, 50, 255},
                                            Color{52, 222, 98, 255}, "button 1");

  const int leftX = static_cast<int>(leftPanel.x + 24.0f);
  int y = static_cast<int>(leftPanel.y + 24.0f);
  DrawText("Steering Input", leftX, y, 26, Color{46, 72, 88, 255});
  y += 44;
  DrawText(TextFormat("axis: %s", hardware_test_view_detail::GetAxisLabel(displayAxis)), leftX, y,
           23, Color{46, 72, 88, 255});
  y += 38;
  DrawText(TextFormat("raw: %.1f deg", sourceAngleDeg), leftX, y, 23, Color{46, 72, 88, 255});
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
  DrawText(TextFormat("green/right: %s", lastGoodFrame.button1Pressed ? "pressed" : "up"), leftX,
           y, 22, Color{46, 72, 88, 255});
  y += 34;
  DrawText(TextFormat("red/left: %s", lastGoodFrame.button2Pressed ? "pressed" : "up"), leftX, y,
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
  DrawText(TextFormat("roll %.1f", lastGoodFrame.roll), rightX, y, 20, Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("pitch %.1f", lastGoodFrame.pitch), rightX, y, 20,
           Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("heading %.1f", lastGoodFrame.heading), rightX, y, 20,
           Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("button1 %s", lastGoodFrame.button1Pressed ? "pressed" : "up"), rightX, y,
           20, Color{46, 72, 88, 255});
  y += 28;
  DrawText(TextFormat("button2 %s", lastGoodFrame.button2Pressed ? "pressed" : "up"), rightX, y,
           20, Color{46, 72, 88, 255});
}
