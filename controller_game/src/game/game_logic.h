#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "city_map.h"

constexpr float kGameMapSize = 1024.0f;
constexpr float kGameCarStartX = 512.0f;
constexpr float kGameCarStartY = 760.0f;
constexpr float kGameCarMargin = 48.0f;
constexpr float kGameCarCollisionRadius = 30.0f;
constexpr float kGameAutoSpeed = 95.0f;
constexpr float kGameMaxSpeed = 190.0f;
constexpr float kGameMaxReverseSpeed = 110.0f;
constexpr float kGameOffroadMaxSpeed = 72.0f;
constexpr float kGameOffroadDrag = 120.0f;
constexpr float kGameManualCoastDrag = 55.0f;
constexpr float kGameManualAcceleration = 165.0f;
constexpr float kGameManualBrake = 245.0f;
constexpr float kGameManualReverseAcceleration = 135.0f;
constexpr float kGameManualReverseEngageDelaySeconds = 1.0f;
constexpr float kGameStoppedSpeedThreshold = 4.0f;
constexpr float kGameMaxTurnRateDegPerSecond = 135.0f;
constexpr float kGameMaxVisualWheelTurnDeg = 32.0f;
constexpr float kGameCoinPickupRadius = 58.0f;
constexpr float kGameCoinCollectAnimationSeconds = 0.42f;
constexpr float kGameDegToRad = 0.017453292519943295769f;
constexpr float kGameRadToDeg = 57.295779513082320876f;
constexpr float kCityRoadHalfWidth = kCityTileSize * 0.25f;
constexpr float kCityCurveRoadRadius = kCityTileSize * 0.5f;

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
  float visualWheelTurnDeg = 0.0f;
  float carSpeed = kGameAutoSpeed;
  float stoppedHoldSeconds = 0.0f;
  DriveMode driveMode = DriveMode::kAuto;
  int score = 0;
  bool onRoad = true;
  bool hitObstacle = false;
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

inline bool HasCityGameplay(const CityMap* city) {
  return city != nullptr && city->columns > 0 && city->rows > 0;
}

inline float CityMapWidth(const CityMap& city) {
  return std::max(1, city.columns) * kCityTileSize;
}

inline float CityMapHeight(const CityMap& city) {
  return std::max(1, city.rows) * kCityTileSize;
}

inline void InitializeGameFromCity(GameState* game, CityMap* city) {
  if (!HasCityGameplay(city)) {
    return;
  }

  if (city->hasPlayerSpawn) {
    game->carPosition = GameVec2{city->playerSpawnX, city->playerSpawnY};
  }
  game->score = 0;
  game->onRoad = true;
  game->hitObstacle = false;
  for (CityCoin& coin : city->coins) {
    coin.collected = false;
    coin.collectAnimationSeconds = 0.0f;
  }
}

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

inline float DistanceSquared(float ax, float ay, float bx, float by) {
  const float dx = ax - bx;
  const float dy = ay - by;
  return dx * dx + dy * dy;
}

inline bool PointInRect(float x, float y, float rectX, float rectY, float width, float height) {
  return x >= rectX && x <= rectX + width && y >= rectY && y <= rectY + height;
}

inline bool CityRoadTileContainsPoint(const CityRoadTile& road, float worldX, float worldY) {
  const float tileX = road.column * kCityTileSize;
  const float tileY = road.row * kCityTileSize;
  const float localX = worldX - tileX;
  const float localY = worldY - tileY;
  if (!PointInRect(localX, localY, 0.0f, 0.0f, kCityTileSize, kCityTileSize)) {
    return false;
  }

  const float center = kCityTileSize * 0.5f;
  const float inner = kCityCurveRoadRadius - kCityRoadHalfWidth;
  const float outer = kCityCurveRoadRadius + kCityRoadHalfWidth;
  switch (road.kind) {
    case CityRoadKind::kHorizontal:
      return std::fabs(localY - center) <= kCityRoadHalfWidth;
    case CityRoadKind::kVertical:
      return std::fabs(localX - center) <= kCityRoadHalfWidth;
    case CityRoadKind::kIntersection:
      return std::fabs(localY - center) <= kCityRoadHalfWidth ||
             std::fabs(localX - center) <= kCityRoadHalfWidth;
    case CityRoadKind::kCurveBottomRight: {
      const float distanceSq = DistanceSquared(localX, localY, kCityTileSize, kCityTileSize);
      return distanceSq >= inner * inner && distanceSq <= outer * outer;
    }
    case CityRoadKind::kCurveBottomLeft: {
      const float distanceSq = DistanceSquared(localX, localY, 0.0f, kCityTileSize);
      return distanceSq >= inner * inner && distanceSq <= outer * outer;
    }
    case CityRoadKind::kCurveTopRight: {
      const float distanceSq = DistanceSquared(localX, localY, kCityTileSize, 0.0f);
      return distanceSq >= inner * inner && distanceSq <= outer * outer;
    }
    case CityRoadKind::kCurveTopLeft: {
      const float distanceSq = DistanceSquared(localX, localY, 0.0f, 0.0f);
      return distanceSq >= inner * inner && distanceSq <= outer * outer;
    }
  }

  return false;
}

inline bool IsPointOnCityRoad(const CityMap& city, GameVec2 point) {
  for (const CityRoadTile& road : city.roads) {
    if (CityRoadTileContainsPoint(road, point.x, point.y)) {
      return true;
    }
  }
  return false;
}

inline bool IsCarOnCityRoad(const CityMap& city, const GameState& game) {
  const float headingRad = game.carHeadingDeg * kGameDegToRad;
  const GameVec2 forward = {std::sin(headingRad), -std::cos(headingRad)};
  const GameVec2 front = {game.carPosition.x + forward.x * 28.0f,
                          game.carPosition.y + forward.y * 28.0f};
  const GameVec2 rear = {game.carPosition.x - forward.x * 26.0f,
                         game.carPosition.y - forward.y * 26.0f};
  return IsPointOnCityRoad(city, game.carPosition) || IsPointOnCityRoad(city, front) ||
         IsPointOnCityRoad(city, rear);
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

inline void CollectNearbyCityCoins(GameState* game, CityMap* city) {
  const float pickupDistanceSq = kGameCoinPickupRadius * kGameCoinPickupRadius;
  game->score = 0;
  for (CityCoin& coin : city->coins) {
    if (!coin.collected &&
        DistanceSquared(game->carPosition.x, game->carPosition.y, coin.x, coin.y) <=
            pickupDistanceSq) {
      coin.collected = true;
      coin.collectAnimationSeconds = 0.0f;
    }
    if (coin.collected) {
      ++game->score;
    }
  }
}

inline void UpdateCityCoinAnimations(CityMap* city, float dt) {
  for (CityCoin& coin : city->coins) {
    if (coin.collected && coin.collectAnimationSeconds < kGameCoinCollectAnimationSeconds) {
      coin.collectAnimationSeconds =
          std::min(kGameCoinCollectAnimationSeconds, coin.collectAnimationSeconds + dt);
    }
  }
}

inline bool ResolveCircleVsCircle(float* x, float* y, float carRadius,
                                  const CityObstacle& obstacle) {
  const float obstacleRadius = obstacle.width * 0.5f;
  const float obstacleCenterX = obstacle.x + obstacleRadius;
  const float obstacleCenterY = obstacle.y + obstacleRadius;
  float dx = *x - obstacleCenterX;
  float dy = *y - obstacleCenterY;
  float distanceSq = dx * dx + dy * dy;
  const float minDistance = carRadius + obstacleRadius;
  if (distanceSq >= minDistance * minDistance) {
    return false;
  }

  if (distanceSq <= 0.0001f) {
    dx = 1.0f;
    dy = 0.0f;
    distanceSq = 1.0f;
  }

  const float distance = std::sqrt(distanceSq);
  const float push = minDistance - distance;
  *x += (dx / distance) * push;
  *y += (dy / distance) * push;
  return true;
}

inline bool ResolveCircleVsRect(float* x, float* y, float carRadius,
                                const CityObstacle& obstacle) {
  const float closestX = std::clamp(*x, obstacle.x, obstacle.x + obstacle.width);
  const float closestY = std::clamp(*y, obstacle.y, obstacle.y + obstacle.height);
  float dx = *x - closestX;
  float dy = *y - closestY;
  float distanceSq = dx * dx + dy * dy;
  if (distanceSq >= carRadius * carRadius && distanceSq > 0.0001f) {
    return false;
  }

  if (distanceSq <= 0.0001f) {
    const float left = std::fabs(*x - obstacle.x);
    const float right = std::fabs((obstacle.x + obstacle.width) - *x);
    const float top = std::fabs(*y - obstacle.y);
    const float bottom = std::fabs((obstacle.y + obstacle.height) - *y);
    const float nearest = std::min(std::min(left, right), std::min(top, bottom));
    if (nearest == left) {
      *x = obstacle.x - carRadius;
    } else if (nearest == right) {
      *x = obstacle.x + obstacle.width + carRadius;
    } else if (nearest == top) {
      *y = obstacle.y - carRadius;
    } else {
      *y = obstacle.y + obstacle.height + carRadius;
    }
    return true;
  }

  const float distance = std::sqrt(distanceSq);
  const float push = carRadius - distance;
  *x += (dx / distance) * push;
  *y += (dy / distance) * push;
  return true;
}

inline bool ResolveCityObstacleCollisions(GameState* game, const CityMap& city) {
  bool hit = false;
  for (const CityObstacle& obstacle : city.obstacles) {
    const bool resolved =
        obstacle.circle
            ? ResolveCircleVsCircle(&game->carPosition.x, &game->carPosition.y,
                                    kGameCarCollisionRadius, obstacle)
            : ResolveCircleVsRect(&game->carPosition.x, &game->carPosition.y,
                                  kGameCarCollisionRadius, obstacle);
    hit = hit || resolved;
  }
  return hit;
}

inline float ComputeSteeringSpeedFactor(float carSpeed) {
  const float normalizedSpeed = std::clamp(std::fabs(carSpeed) / kGameMaxSpeed, 0.0f, 1.0f);
  return 1.05f - normalizedSpeed * 0.30f;
}

inline float ComputeLowSpeedYawFactor(float carSpeed) {
  const float normalizedSpeed = std::clamp(std::fabs(carSpeed) / 28.0f, 0.0f, 1.0f);
  return normalizedSpeed * normalizedSpeed;
}

inline bool IsNearlyStopped(float carSpeed) {
  return std::fabs(carSpeed) <= kGameStoppedSpeedThreshold;
}

inline void UpdateGame(GameState* game, float steeringInput, GameButtons buttons, float dt,
                       CityMap* city = nullptr) {
  if (dt <= 0.0f) {
    return;
  }

  const float clampedSteering = ClampUnit(steeringInput);
  game->visualWheelTurnDeg = clampedSteering * kGameMaxVisualWheelTurnDeg;
  if (game->driveMode == DriveMode::kAuto) {
    const float speedBlend = std::min(dt * 4.0f, 1.0f);
    game->carSpeed += (kGameAutoSpeed - game->carSpeed) * speedBlend;
    game->stoppedHoldSeconds = 0.0f;
  } else {
    if (buttons.accelerate) {
      game->carSpeed += kGameManualAcceleration * dt;
      game->stoppedHoldSeconds = 0.0f;
    }

    if (buttons.brake) {
      if (game->carSpeed > 0.0f) {
        game->carSpeed -= kGameManualBrake * dt;
        if (game->carSpeed < 0.0f) {
          game->carSpeed = 0.0f;
        }
      } else if (IsNearlyStopped(game->carSpeed)) {
        game->carSpeed = 0.0f;
        if (game->stoppedHoldSeconds >= kGameManualReverseEngageDelaySeconds) {
          game->carSpeed -= kGameManualReverseAcceleration * dt;
        }
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
    if (buttons.accelerate) {
      game->stoppedHoldSeconds = 0.0f;
    } else if (IsNearlyStopped(game->carSpeed)) {
      game->carSpeed = 0.0f;
      game->stoppedHoldSeconds += dt;
    } else {
      game->stoppedHoldSeconds = 0.0f;
    }
  }

  const float speedFactor = ComputeSteeringSpeedFactor(game->carSpeed);
  const float lowSpeedYawFactor = ComputeLowSpeedYawFactor(game->carSpeed);
  const float steeringDirectionFromTravel = game->carSpeed < 0.0f ? -1.0f : 1.0f;
  game->carHeadingDeg +=
      clampedSteering * steeringDirectionFromTravel * kGameMaxTurnRateDegPerSecond * speedFactor *
      lowSpeedYawFactor * dt;

  if (HasCityGameplay(city)) {
    game->onRoad = IsCarOnCityRoad(*city, *game);
    if (!game->onRoad) {
      if (game->carSpeed > kGameOffroadMaxSpeed) {
        game->carSpeed = std::max(kGameOffroadMaxSpeed, game->carSpeed - kGameOffroadDrag * dt);
      } else if (game->carSpeed < -kGameOffroadMaxSpeed) {
        game->carSpeed = std::min(-kGameOffroadMaxSpeed, game->carSpeed + kGameOffroadDrag * dt);
      }
    }
  } else {
    game->onRoad = true;
  }

  const float headingRad = game->carHeadingDeg * kGameDegToRad;
  game->carPosition.x += std::sin(headingRad) * game->carSpeed * dt;
  game->carPosition.y -= std::cos(headingRad) * game->carSpeed * dt;

  if (HasCityGameplay(city)) {
    game->carPosition.x =
        std::clamp(game->carPosition.x, kGameCarMargin, CityMapWidth(*city) - kGameCarMargin);
    game->carPosition.y =
        std::clamp(game->carPosition.y, kGameCarMargin, CityMapHeight(*city) - kGameCarMargin);
    game->hitObstacle = ResolveCityObstacleCollisions(game, *city);
    if (game->hitObstacle) {
      game->carSpeed *= -0.18f;
    }
    game->onRoad = IsCarOnCityRoad(*city, *game);
    CollectNearbyCityCoins(game, city);
    UpdateCityCoinAnimations(city, dt);
  } else {
    game->carPosition.x =
        std::clamp(game->carPosition.x, kGameCarMargin, kGameMapSize - kGameCarMargin);
    game->carPosition.y =
        std::clamp(game->carPosition.y, kGameCarMargin, kGameMapSize - kGameCarMargin);
    game->hitObstacle = false;
    CollectNearbyCoins(game);
  }
}
