#include "game_logic.h"

#include <cassert>
#include <cmath>

namespace {
void TestPacketParser() {
  SensorFrame frame;
  assert(!ParsePacket("roll=1.0,pitch=2.0,heading=3.0", &frame));
  assert(ParsePacket("qw=0.7071,qx=0.0,qy=0.7071,qz=0.0,button1=1,button2=0", &frame));
  assert(std::fabs(frame.orientation.w - 0.7071f) < 0.001f);
  assert(std::fabs(frame.orientation.x - 0.0f) < 0.001f);
  assert(std::fabs(frame.orientation.y - 0.7071f) < 0.001f);
  assert(std::fabs(frame.orientation.z - 0.0f) < 0.001f);
  assert(frame.button1Pressed);
  assert(!frame.button2Pressed);
}

void TestCenteredPitchTwist() {
  const SensorFrame center = {SensorQuaternion{1.0f, 0.0f, 0.0f, 0.0f}, false, false};
  const float halfTurn = std::sqrt(0.5f);
  const SensorFrame turned = {SensorQuaternion{halfTurn, 0.0f, halfTurn, 0.0f}, false, false};
  const float twistDeg = GetCenteredAxisDegrees(OrientationAxis::kPitch, turned, center);
  assert(std::fabs(twistDeg - 90.0f) < 0.5f);
}

void TestAutoDriveAdvancesCar() {
  GameState game;
  const GameVec2 start = game.carPosition;
  UpdateGame(&game, 0.0f, {}, 1.0f);
  assert(game.carPosition.y < start.y);
  assert(game.carSpeed > 0.0f);
}

void TestManualThrottleAndBrake() {
  GameState game;
  ToggleDriveMode(&game);
  game.carSpeed = 0.0f;
  UpdateGame(&game, 0.0f, GameButtons{true, false}, 1.0f);
  assert(game.carSpeed > 100.0f);
  const float acceleratedSpeed = game.carSpeed;
  UpdateGame(&game, 0.0f, GameButtons{false, true}, 0.5f);
  assert(game.carSpeed < acceleratedSpeed);
}

void TestManualBrakeCanReverse() {
  GameState game;
  ToggleDriveMode(&game);
  game.carSpeed = 0.0f;
  UpdateGame(&game, 0.0f, GameButtons{false, false}, 0.5f);
  assert(game.carSpeed == 0.0f);
  UpdateGame(&game, 0.0f, GameButtons{false, false}, 0.6f);
  assert(game.carSpeed == 0.0f);
  UpdateGame(&game, 0.0f, GameButtons{false, true}, 0.1f);
  assert(game.carSpeed < 0.0f);
}

void TestManualBrakeStopsBeforeReverse() {
  GameState game;
  ToggleDriveMode(&game);
  game.carSpeed = 50.0f;
  UpdateGame(&game, 0.0f, GameButtons{false, true}, 0.25f);
  assert(game.carSpeed == 0.0f);
  UpdateGame(&game, 0.0f, GameButtons{false, false}, 0.75f);
  assert(game.carSpeed == 0.0f);
  UpdateGame(&game, 0.0f, GameButtons{false, true}, 0.3f);
  assert(game.carSpeed < 0.0f);
}

void TestDriveModeToggle() {
  GameState game;
  assert(game.driveMode == DriveMode::kAuto);
  ToggleDriveMode(&game);
  assert(game.driveMode == DriveMode::kManual);
  ToggleDriveMode(&game);
  assert(game.driveMode == DriveMode::kAuto);
}

void TestCoinCollection() {
  GameState game;
  game.carPosition = game.coins[0].position;
  CollectNearbyCoins(&game);
  assert(game.coins[0].collected);
  assert(game.score == 1);
}

void TestSteeringSpeedFactorDropsAtSpeed() {
  assert(ComputeSteeringSpeedFactor(0.0f) > ComputeSteeringSpeedFactor(kGameMaxSpeed));
  assert(ComputeSteeringSpeedFactor(kGameMaxSpeed) >= 0.70f);
}

void TestLowSpeedYawFactorNearZeroWhenStopped() {
  assert(ComputeLowSpeedYawFactor(0.0f) == 0.0f);
  assert(ComputeLowSpeedYawFactor(28.0f) >= 0.99f);
}

void TestReverseSteeringTurnsOppositeDirection() {
  GameState game;
  ToggleDriveMode(&game);
  game.carSpeed = -50.0f;
  const float startHeading = game.carHeadingDeg;
  UpdateGame(&game, 1.0f, GameButtons{false, true}, 0.25f);
  assert(game.carHeadingDeg < startHeading);
}
}  // namespace

int main() {
  TestPacketParser();
  TestCenteredPitchTwist();
  TestAutoDriveAdvancesCar();
  TestManualThrottleAndBrake();
  TestManualBrakeCanReverse();
  TestManualBrakeStopsBeforeReverse();
  TestDriveModeToggle();
  TestCoinCollection();
  TestSteeringSpeedFactorDropsAtSpeed();
  TestLowSpeedYawFactorNearZeroWhenStopped();
  TestReverseSteeringTurnsOppositeDirection();
  return 0;
}
