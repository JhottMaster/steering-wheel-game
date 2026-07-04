#pragma once

#include <cmath>

#include "raylib.h"

namespace steering_wheel_2d {
constexpr float kDegToRad = 0.017453292519943295769f;

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
}  // namespace steering_wheel_2d
