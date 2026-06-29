#include "game_logic.h"

#include <cassert>
#include <cmath>

namespace {
void TestPacketParser() {
  SensorFrame frame;
  assert(!ParsePacket("roll=1.0,pitch=2.0,heading=3.0", &frame));
  assert(ParsePacket("roll=1.5,pitch=-2.0,heading=33.0,button1=1,button2=0", &frame));
  assert(std::fabs(frame.roll - 1.5f) < 0.001f);
  assert(std::fabs(frame.pitch + 2.0f) < 0.001f);
  assert(std::fabs(frame.heading - 33.0f) < 0.001f);
  assert(frame.button1Pressed);
  assert(!frame.button2Pressed);
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
}  // namespace

int main() {
  TestPacketParser();
  TestAutoDriveAdvancesCar();
  TestManualThrottleAndBrake();
  TestDriveModeToggle();
  TestCoinCollection();
  return 0;
}
