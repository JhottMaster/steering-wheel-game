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
constexpr float kGameManualCoastDrag = 55.0f;
constexpr float kGameManualAcceleration = 165.0f;
constexpr float kGameManualBrake = 245.0f;
constexpr float kGameMaxTurnRateDegPerSecond = 135.0f;
constexpr float kGameCoinPickupRadius = 58.0f;
constexpr float kGameDegToRad = 0.017453292519943295769f;

struct SensorFrame {
  float roll = 0.0f;
  float pitch = 0.0f;
  float heading = 0.0f;
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
  float roll = 0.0f;
  float pitch = 0.0f;
  float heading = 0.0f;
  int button1 = 0;
  int button2 = 0;
  const int parsed =
      std::sscanf(packet, "roll=%f,pitch=%f,heading=%f,button1=%d,button2=%d",
                  &roll, &pitch, &heading, &button1, &button2);
  if (parsed != 5) {
    return false;
  }

  frame->roll = roll;
  frame->pitch = pitch;
  frame->heading = heading;
  frame->button1Pressed = button1 != 0;
  frame->button2Pressed = button2 != 0;
  return true;
}

inline float ClampUnit(float value) {
  return std::clamp(value, -1.0f, 1.0f);
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
    } else {
      game->carSpeed -= kGameManualCoastDrag * dt;
    }

    if (buttons.brake) {
      game->carSpeed -= kGameManualBrake * dt;
    }
    game->carSpeed = std::clamp(game->carSpeed, 0.0f, kGameMaxSpeed);
  }

  const float speedFactor = std::clamp(game->carSpeed / kGameAutoSpeed, 0.25f, 1.8f);
  game->carHeadingDeg += clampedSteering * kGameMaxTurnRateDegPerSecond * speedFactor * dt;

  const float headingRad = game->carHeadingDeg * kGameDegToRad;
  game->carPosition.x += std::sin(headingRad) * game->carSpeed * dt;
  game->carPosition.y -= std::cos(headingRad) * game->carSpeed * dt;
  game->carPosition.x = std::clamp(game->carPosition.x, kGameCarMargin, kGameMapSize - kGameCarMargin);
  game->carPosition.y = std::clamp(game->carPosition.y, kGameCarMargin, kGameMapSize - kGameCarMargin);

  CollectNearbyCoins(game);
}
