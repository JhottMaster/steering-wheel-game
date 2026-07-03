#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

constexpr float kGameMapSize = 1024.0f;
constexpr float kGameCarStartX = 512.0f;
constexpr float kGameCarStartY = 760.0f;
constexpr float kGameCarMargin = 48.0f;
constexpr float kGameAutoSpeed = 95.0f;
constexpr float kGameMaxSpeed = 190.0f;
constexpr float kGameMaxReverseSpeed = 110.0f;
constexpr float kGameManualCoastDrag = 55.0f;
constexpr float kGameManualAcceleration = 165.0f;
constexpr float kGameManualBrake = 245.0f;
constexpr float kGameManualReverseAcceleration = 135.0f;
constexpr float kGameMaxTurnRateDegPerSecond = 135.0f;
constexpr float kGameCoinPickupRadius = 58.0f;
constexpr float kGameDegToRad = 0.017453292519943295769f;
constexpr float kGameRadToDeg = 57.295779513082320876f;

struct SensorQuaternion {
  float w = 1.0f;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
};

struct SensorFrame {
  SensorQuaternion orientation;
  bool button1Pressed = false;
  bool button2Pressed = false;
};

struct GameVec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct CoinState {
  GameVec2 position;
  bool collected = false;
};

enum class DriveMode {
  kAuto,
  kManual,
};

enum class OrientationAxis {
  kRoll,
  kPitch,
  kYaw,
};

struct GameButtons {
  bool accelerate = false;
  bool brake = false;
};

struct GameState {
  GameVec2 carPosition = {kGameCarStartX, kGameCarStartY};
  float carHeadingDeg = 0.0f;
  float carSpeed = kGameAutoSpeed;
  DriveMode driveMode = DriveMode::kAuto;
  int score = 0;
  std::array<CoinState, 8> coins = {{
      {{188.0f, 182.0f}, false},
      {{460.0f, 160.0f}, false},
      {{806.0f, 188.0f}, false},
      {{180.0f, 484.0f}, false},
      {{545.0f, 438.0f}, false},
      {{850.0f, 510.0f}, false},
      {{312.0f, 828.0f}, false},
      {{738.0f, 820.0f}, false},
  }};
};

inline bool ParsePacket(const char* packet, SensorFrame* frame) {
  float qw = 1.0f;
  float qx = 0.0f;
  float qy = 0.0f;
  float qz = 0.0f;
  int button1 = 0;
  int button2 = 0;
  const int parsed =
      std::sscanf(packet, "qw=%f,qx=%f,qy=%f,qz=%f,button1=%d,button2=%d", &qw, &qx, &qy, &qz,
                  &button1, &button2);
  if (parsed != 6) {
    return false;
  }

  frame->orientation = SensorQuaternion{qw, qx, qy, qz};
  frame->button1Pressed = button1 != 0;
  frame->button2Pressed = button2 != 0;
  return true;
}

inline float ClampUnit(float value) {
  return std::clamp(value, -1.0f, 1.0f);
}

inline float WrapDegrees(float degrees) {
  while (degrees > 180.0f) {
    degrees -= 360.0f;
  }
  while (degrees < -180.0f) {
    degrees += 360.0f;
  }
  return degrees;
}

inline float QuaternionLengthSquared(const SensorQuaternion& quaternion) {
  return quaternion.w * quaternion.w + quaternion.x * quaternion.x + quaternion.y * quaternion.y +
         quaternion.z * quaternion.z;
}

inline SensorQuaternion NormalizeQuaternion(const SensorQuaternion& quaternion) {
  const float lengthSquared = QuaternionLengthSquared(quaternion);
  if (lengthSquared <= 0.000001f) {
    return SensorQuaternion{};
  }

  const float invLength = 1.0f / std::sqrt(lengthSquared);
  return SensorQuaternion{quaternion.w * invLength, quaternion.x * invLength,
                          quaternion.y * invLength, quaternion.z * invLength};
}

inline SensorQuaternion ConjugateQuaternion(const SensorQuaternion& quaternion) {
  return SensorQuaternion{quaternion.w, -quaternion.x, -quaternion.y, -quaternion.z};
}

inline SensorQuaternion MultiplyQuaternion(const SensorQuaternion& a, const SensorQuaternion& b) {
  return SensorQuaternion{
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
  };
}

inline SensorQuaternion InverseUnitQuaternion(const SensorQuaternion& quaternion) {
  return ConjugateQuaternion(NormalizeQuaternion(quaternion));
}

inline SensorQuaternion RelativeQuaternion(const SensorQuaternion& current,
                                           const SensorQuaternion& center) {
  return NormalizeQuaternion(MultiplyQuaternion(InverseUnitQuaternion(center),
                                                NormalizeQuaternion(current)));
}

inline float ExtractTwistDegrees(const SensorQuaternion& quaternion, float axisX, float axisY,
                                 float axisZ) {
  const SensorQuaternion normalized = NormalizeQuaternion(quaternion);
  const float projection =
      normalized.x * axisX + normalized.y * axisY + normalized.z * axisZ;
  const SensorQuaternion twist = NormalizeQuaternion(SensorQuaternion{
      normalized.w, axisX * projection, axisY * projection, axisZ * projection});
  const float twistLengthSquared = QuaternionLengthSquared(twist);
  if (twistLengthSquared <= 0.000001f) {
    return 0.0f;
  }

  const float signedComponent = twist.x * axisX + twist.y * axisY + twist.z * axisZ;
  return WrapDegrees(2.0f * std::atan2(signedComponent, twist.w) * kGameRadToDeg);
}

inline float GetAxisDegrees(OrientationAxis axis, const SensorFrame& frame) {
  switch (axis) {
    case OrientationAxis::kRoll:
      return ExtractTwistDegrees(frame.orientation, 1.0f, 0.0f, 0.0f);
    case OrientationAxis::kPitch:
      return ExtractTwistDegrees(frame.orientation, 0.0f, 1.0f, 0.0f);
    case OrientationAxis::kYaw:
      return ExtractTwistDegrees(frame.orientation, 0.0f, 0.0f, 1.0f);
  }

  return ExtractTwistDegrees(frame.orientation, 0.0f, 1.0f, 0.0f);
}

inline float GetCenteredAxisDegrees(OrientationAxis axis, const SensorFrame& frame,
                                    const SensorFrame& centerFrame) {
  const SensorQuaternion relative = RelativeQuaternion(frame.orientation, centerFrame.orientation);
  switch (axis) {
    case OrientationAxis::kRoll:
      return ExtractTwistDegrees(relative, 1.0f, 0.0f, 0.0f);
    case OrientationAxis::kPitch:
      return ExtractTwistDegrees(relative, 0.0f, 1.0f, 0.0f);
    case OrientationAxis::kYaw:
      return ExtractTwistDegrees(relative, 0.0f, 0.0f, 1.0f);
  }

  return ExtractTwistDegrees(relative, 0.0f, 1.0f, 0.0f);
}

inline float QuaternionToRollDegrees(const SensorQuaternion& quaternion) {
  const SensorQuaternion normalized = NormalizeQuaternion(quaternion);
  const float sinrCosp = 2.0f * (normalized.w * normalized.x + normalized.y * normalized.z);
  const float cosrCosp =
      1.0f - 2.0f * (normalized.x * normalized.x + normalized.y * normalized.y);
  return std::atan2(sinrCosp, cosrCosp) * kGameRadToDeg;
}

inline float QuaternionToPitchDegrees(const SensorQuaternion& quaternion) {
  const SensorQuaternion normalized = NormalizeQuaternion(quaternion);
  const float sinp = 2.0f * (normalized.w * normalized.y - normalized.z * normalized.x);
  if (std::fabs(sinp) >= 1.0f) {
    return std::copysign(90.0f, sinp);
  }
  return std::asin(sinp) * kGameRadToDeg;
}

inline float QuaternionToYawDegrees(const SensorQuaternion& quaternion) {
  const SensorQuaternion normalized = NormalizeQuaternion(quaternion);
  const float sinyCosp = 2.0f * (normalized.w * normalized.z + normalized.x * normalized.y);
  const float cosyCosp =
      1.0f - 2.0f * (normalized.y * normalized.y + normalized.z * normalized.z);
  return std::atan2(sinyCosp, cosyCosp) * kGameRadToDeg;
}

inline void ToggleDriveMode(GameState* game) {
  game->driveMode = game->driveMode == DriveMode::kAuto ? DriveMode::kManual : DriveMode::kAuto;
  if (game->driveMode == DriveMode::kAuto && game->carSpeed < kGameAutoSpeed) {
    game->carSpeed = kGameAutoSpeed;
  }
}

inline float DistanceSquared(GameVec2 a, GameVec2 b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return dx * dx + dy * dy;
}

inline void CollectNearbyCoins(GameState* game) {
  const float pickupDistanceSq = kGameCoinPickupRadius * kGameCoinPickupRadius;
  for (CoinState& coin : game->coins) {
    if (!coin.collected && DistanceSquared(game->carPosition, coin.position) <= pickupDistanceSq) {
      coin.collected = true;
      ++game->score;
    }
  }
}

inline void UpdateGame(GameState* game, float steeringInput, GameButtons buttons, float dt) {
  if (dt <= 0.0f) {
    return;
  }

  const float clampedSteering = ClampUnit(steeringInput);
  if (game->driveMode == DriveMode::kAuto) {
    const float speedBlend = std::min(dt * 4.0f, 1.0f);
    game->carSpeed += (kGameAutoSpeed - game->carSpeed) * speedBlend;
  } else {
    if (buttons.accelerate) {
      game->carSpeed += kGameManualAcceleration * dt;
    }

    if (buttons.brake) {
      if (game->carSpeed > 0.0f) {
        game->carSpeed -= kGameManualBrake * dt;
      } else {
        game->carSpeed -= kGameManualReverseAcceleration * dt;
      }
    }

    if (!buttons.accelerate && !buttons.brake) {
      if (game->carSpeed > 0.0f) {
        game->carSpeed = std::max(0.0f, game->carSpeed - kGameManualCoastDrag * dt);
      } else if (game->carSpeed < 0.0f) {
        game->carSpeed = std::min(0.0f, game->carSpeed + kGameManualCoastDrag * dt);
      }
    }
    game->carSpeed = std::clamp(game->carSpeed, -kGameMaxReverseSpeed, kGameMaxSpeed);
  }

  const float speedFactor = std::clamp(std::fabs(game->carSpeed) / kGameAutoSpeed, 0.25f, 1.8f);
  game->carHeadingDeg += clampedSteering * kGameMaxTurnRateDegPerSecond * speedFactor * dt;

  const float headingRad = game->carHeadingDeg * kGameDegToRad;
  game->carPosition.x += std::sin(headingRad) * game->carSpeed * dt;
  game->carPosition.y -= std::cos(headingRad) * game->carSpeed * dt;
  game->carPosition.x =
      std::clamp(game->carPosition.x, kGameCarMargin, kGameMapSize - kGameCarMargin);
  game->carPosition.y =
      std::clamp(game->carPosition.y, kGameCarMargin, kGameMapSize - kGameCarMargin);

  CollectNearbyCoins(game);
}
