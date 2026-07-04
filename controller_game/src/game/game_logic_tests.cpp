#include "game_logic.h"
#include "road_art_tuning.h"
#include "../app/pause_menu.h"
#include "../app/server_discovery.h"
#include "../input/datagram_receive.h"
#include "../input/sensor_receiver.h"
#include "../input/steering_input.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <queue>
#include <string>
#include <vector>

namespace {
struct FakeDatagram {
  DatagramReceiveStatus status = DatagramReceiveStatus::kWouldBlock;
  std::string payload;
};

struct FakeReceiver {
  std::queue<FakeDatagram> datagrams;

  DatagramReceiveStatus ReceiveDatagram(char* buffer, int capacity, int* receivedBytes) {
    if (datagrams.empty()) {
      return DatagramReceiveStatus::kWouldBlock;
    }

    const FakeDatagram datagram = datagrams.front();
    datagrams.pop();
    if (datagram.status != DatagramReceiveStatus::kPacket) {
      return datagram.status;
    }

    const int bytesToCopy = std::min(capacity, static_cast<int>(datagram.payload.size()));
    std::copy(datagram.payload.begin(), datagram.payload.begin() + bytesToCopy, buffer);
    *receivedBytes = bytesToCopy;
    return DatagramReceiveStatus::kPacket;
  }
};

struct FakeBroadcaster {
  int sendCount = 0;
  std::string lastPayload;
  int lastPort = 0;

  bool SendBroadcast(const char* payload, int payloadLength, int port) {
    ++sendCount;
    lastPayload.assign(payload, payload + payloadLength);
    lastPort = port;
    return true;
  }
};

SensorFrame MakePitchFrame(float degrees) {
  const float halfRadians = degrees * kGameDegToRad * 0.5f;
  return SensorFrame{SensorQuaternion{std::cos(halfRadians), 0.0f, std::sin(halfRadians), 0.0f},
                     false, false};
}

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

void TestSensorReceiverKeepsLatestValidPacket() {
  FakeReceiver receiver;
  receiver.datagrams.push(
      FakeDatagram{DatagramReceiveStatus::kPacket,
                   "qw=1.0,qx=0.0,qy=0.0,qz=0.0,button1=0,button2=0"});
  receiver.datagrams.push(FakeDatagram{DatagramReceiveStatus::kPacket, "invalid"});
  receiver.datagrams.push(
      FakeDatagram{DatagramReceiveStatus::kPacket,
                   "qw=0.5,qx=0.0,qy=0.5,qz=0.0,button1=1,button2=1"});
  receiver.datagrams.push(FakeDatagram{DatagramReceiveStatus::kWouldBlock, ""});

  SensorFrame frame;
  assert(PollLatestSensorFrame(&receiver, &frame));
  assert(std::fabs(frame.orientation.w - 0.5f) < 0.001f);
  assert(std::fabs(frame.orientation.y - 0.5f) < 0.001f);
  assert(frame.button1Pressed);
  assert(frame.button2Pressed);
}

void TestSensorReceiverReturnsFalseWhenOnlyInvalidPacketsArrive() {
  FakeReceiver receiver;
  receiver.datagrams.push(FakeDatagram{DatagramReceiveStatus::kPacket, "invalid"});
  receiver.datagrams.push(FakeDatagram{DatagramReceiveStatus::kWouldBlock, ""});

  SensorFrame frame;
  assert(!PollLatestSensorFrame(&receiver, &frame));
}

void TestCenteredPitchTwist() {
  const SensorFrame center = {SensorQuaternion{1.0f, 0.0f, 0.0f, 0.0f}, false, false};
  const float halfTurn = std::sqrt(0.5f);
  const SensorFrame turned = {SensorQuaternion{halfTurn, 0.0f, halfTurn, 0.0f}, false, false};
  const float twistDeg = GetCenteredAxisDegrees(OrientationAxis::kPitch, turned, center);
  assert(std::fabs(twistDeg - 90.0f) < 0.5f);
}

void TestSteeringInputWraparound() {
  SteeringInputState steering;
  ResetControllerCenter(MakePitchFrame(0.0f), true, &steering);
  RecordSensorFrameForSteering(MakePitchFrame(170.0f), &steering);
  assert(std::fabs(steering.accumulatedGameAngleDeg - 170.0f) < 0.5f);
  RecordSensorFrameForSteering(MakePitchFrame(-170.0f), &steering);
  assert(std::fabs(steering.accumulatedGameAngleDeg - 190.0f) < 0.5f);
}

void TestKeyboardSteeringFallback() {
  SteeringInputState steering;
  UpdateKeyboardSteeringFallback(&steering, 1.0f, false, 0.1f);
  assert(steering.manualAngleDeg > 20.0f);
  assert(steering.manualAngleDeg < kSteeringKeyboardFallbackFullLockDeg);
  UpdateKeyboardSteeringFallback(&steering, 0.0f, false, 1.0f);
  assert(steering.manualAngleDeg == 0.0f);
  steering.manualAngleDeg = 12.0f;
  UpdateKeyboardSteeringFallback(&steering, 1.0f, true, 0.1f);
  assert(steering.manualAngleDeg == 0.0f);
}

void TestPauseMenuSteeringAndButtons() {
  PauseMenuState menu;
  OpenPauseMenu(&menu);
  GameState game;
  assert(GetSelectedPauseMenuItem(menu) == PauseMenuItem::kResume);

  assert(UpdatePauseMenu(&menu, kPauseMenuSteerThresholdDeg + 1.0f, false, false, 0.016f) ==
         PauseMenuAction::kNone);
  assert(GetSelectedPauseMenuItem(menu) == PauseMenuItem::kRestart);
  assert(UpdatePauseMenu(&menu, kPauseMenuSteerThresholdDeg + 1.0f, false, false, 0.016f) ==
         PauseMenuAction::kNone);
  assert(GetSelectedPauseMenuItem(menu) == PauseMenuItem::kRestart);
  assert(UpdatePauseMenu(&menu, 0.0f, false, false, 0.016f) == PauseMenuAction::kNone);
  assert(UpdatePauseMenu(&menu, 0.0f, true, false, 0.016f) == PauseMenuAction::kRestart);
  assert(GetPauseMenuItemLabel(PauseMenuItem::kToggleDriveMode, game, AppMode::kGame) ==
         std::string("Mode: Button Gas"));
}

void TestPauseChordOpensMenu() {
  PauseMenuState menu;
  assert(!menu.active);
  assert(UpdatePauseMenu(&menu, 0.0f, true, true, kPauseChordHoldSeconds * 0.5f) ==
         PauseMenuAction::kNone);
  assert(!menu.active);
  assert(UpdatePauseMenu(&menu, 0.0f, true, true, kPauseChordHoldSeconds * 0.6f) ==
         PauseMenuAction::kNone);
  assert(menu.active);
}

void TestServerDiscoveryBeacon() {
  using Clock = std::chrono::steady_clock;
  const Clock::time_point start = Clock::time_point{} + std::chrono::seconds(30);
  FakeBroadcaster broadcaster;
  DiscoveryBeaconState discovery;

  UpdateServerDiscoveryBeacon(&discovery, &broadcaster, true, false, {}, start, false);
  assert(broadcaster.sendCount == 1);
  assert(broadcaster.lastPayload == kServerDiscoveryMessage);
  assert(broadcaster.lastPort == kServerDiscoveryPort);

  UpdateServerDiscoveryBeacon(&discovery, &broadcaster, true, false, {},
                              start + std::chrono::seconds(1), false);
  assert(broadcaster.sendCount == 1);

  MarkControllerPacketReceived(&discovery);
  const Clock::time_point packetTime = start + std::chrono::seconds(2);
  UpdateServerDiscoveryBeacon(&discovery, &broadcaster, true, true, packetTime,
                              packetTime + std::chrono::seconds(1), false);
  assert(broadcaster.sendCount == 1);
  assert(!discovery.wasBroadcastingForLoss);

  UpdateServerDiscoveryBeacon(&discovery, &broadcaster, true, false, packetTime,
                              packetTime + std::chrono::seconds(6), false);
  assert(broadcaster.sendCount == 2);
  assert(discovery.wasBroadcastingForLoss);
}

void TestAutoDriveAdvancesCar() {
  GameState game;
  ToggleDriveMode(&game);
  const GameVec2 start = game.carPosition;
  UpdateGame(&game, 0.0f, {}, 1.0f);
  assert(game.carPosition.y < start.y);
  assert(game.carSpeed > 0.0f);
}

void TestManualThrottleAndBrake() {
  GameState game;
  game.carSpeed = 0.0f;
  UpdateGame(&game, 0.0f, GameButtons{true, false}, 1.0f);
  assert(game.carSpeed > 100.0f);
  const float acceleratedSpeed = game.carSpeed;
  UpdateGame(&game, 0.0f, GameButtons{false, true}, 0.5f);
  assert(game.carSpeed < acceleratedSpeed);
}

void TestManualBrakeCanReverse() {
  GameState game;
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
  assert(game.driveMode == DriveMode::kManual);
  ToggleDriveMode(&game);
  assert(game.driveMode == DriveMode::kAuto);
  ToggleDriveMode(&game);
  assert(game.driveMode == DriveMode::kManual);
}

void TestCoinCollection() {
  GameState game;
  game.carPosition = game.coins[0].position;
  CollectNearbyCoins(&game);
  assert(game.coins[0].collected);
  assert(game.score == 1);
}

void TestCitySpawnAndCoinCollection() {
  CityMap city;
  city.columns = 4;
  city.rows = 4;
  city.hasPlayerSpawn = true;
  city.playerSpawnX = 300.0f;
  city.playerSpawnY = 400.0f;
  city.coins.push_back(CityCoin{300.0f, 400.0f, false, 0.0f});

  GameState game;
  InitializeGameFromCity(&game, &city);
  assert(game.carPosition.x == 300.0f);
  assert(game.carPosition.y == 400.0f);
  UpdateGame(&game, 0.0f, {}, 0.01f, &city);
  assert(city.coins[0].collected);
  assert(city.coins[0].collectAnimationSeconds > 0.0f);
  assert(game.score == 1);
}

void TestCityKrakenCollection() {
  CityMap city;
  city.columns = 4;
  city.rows = 4;
  city.hasPlayerSpawn = true;
  city.playerSpawnX = 300.0f;
  city.playerSpawnY = 400.0f;
  city.krakens.push_back(CityKraken{CityKrakenKind::kRoad, 300.0f, 400.0f, 180.0f, false, 0.0f});

  GameState game;
  InitializeGameFromCity(&game, &city);
  UpdateGame(&game, 0.0f, {}, 0.01f, &city);
  assert(city.krakens[0].collected);
  assert(city.krakens[0].collectAnimationSeconds > 0.0f);
  assert(game.krakensCollected == 1);
  assert(game.score == 0);
}

void TestCityRoadDetection() {
  CityMap city;
  city.columns = 2;
  city.rows = 2;
  city.roads.push_back(CityRoadTile{CityRoadKind::kHorizontal, 0, 0});
  const float center = kCityTileSize * 0.5f;
  assert(IsPointOnCityRoad(city, GameVec2{center, center}));
  assert(!IsPointOnCityRoad(city, GameVec2{center, 8.0f}));
}

void TestCityMapParserWarnings() {
  const std::string path = "build/test_city_map.csv";
  {
    std::ofstream file(path);
    file << "r_h|coin:star|mystery_token,spawn:player@256:256\n";
  }

  std::vector<std::string> warnings;
  CityMap city = LoadCityMap(path, &warnings);
  assert(city.columns == 2);
  assert(city.rows == 1);
  assert(city.roads.size() == 1);
  assert(city.coins.size() == 1);
  assert(city.hasPlayerSpawn);
  assert(std::fabs(city.playerSpawnX - kCityTileSize * 1.5f) < 0.001f);
  assert(std::fabs(city.playerSpawnY - kCityTileSize * 0.5f) < 0.001f);
  assert(warnings.size() == 1);
  assert(warnings[0].find("mystery_token") != std::string::npos);

  std::remove(path.c_str());
}

void TestRoadArtTuningWarningsAndClamps() {
  const std::string path = "build/test_road_art_tuning.csv";
  {
    std::ofstream file(path);
    file << "sprite,scale_percent,footprint_percent,offset_x,offset_y,anchor_x,anchor_y,draw_mode\n";
    file << "road_horizontal,350,0,5,6,99,-99,repeat_horizontal\n";
    file << "road_vertical,bad,100,0,0,0,0,repeat_vertical\n";
    file << "not_a_sprite,100,100,0,0,0,0,stretch\n";
    file << "road_curve_top_left,100\n";
  }

  std::vector<std::string> warnings;
  RoadArtTuning tuning = LoadRoadArtTuning(path, &warnings);
  assert(tuning.horizontal.scalePercent == 300);
  assert(tuning.horizontal.footprintPercent == 1);
  assert(tuning.horizontal.offsetX == 5);
  assert(tuning.horizontal.offsetY == 6);
  assert(tuning.horizontal.anchorX == kRoadArtAnchorMax);
  assert(tuning.horizontal.anchorY == kRoadArtAnchorMin);
  assert(tuning.horizontal.drawMode == RoadArtDrawMode::kRepeatHorizontal);
  assert(warnings.size() == 3);
  assert(warnings[0].find("scale_percent") != std::string::npos);
  assert(warnings[1].find("not_a_sprite") != std::string::npos);
  assert(warnings[2].find("columns") != std::string::npos);

  assert(SaveRoadArtTuning(path, tuning));
  std::vector<std::string> roundTripWarnings;
  RoadArtTuning roundTrip = LoadRoadArtTuning(path, &roundTripWarnings);
  assert(roundTrip.horizontal.scalePercent == 300);
  assert(roundTrip.horizontal.footprintPercent == 1);
  assert(roundTrip.horizontal.anchorX == kRoadArtAnchorMax);
  assert(roundTrip.horizontal.anchorY == kRoadArtAnchorMin);
  assert(roundTripWarnings.empty());

  std::remove(path.c_str());
}

void TestCityObstaclePushesCar() {
  CityMap city;
  city.columns = 2;
  city.rows = 2;
  const float center = kCityTileSize * 0.5f;
  const float obstacleSize = kCityTileSize * 0.35f;
  city.obstacles.push_back(
      CityObstacle{center - obstacleSize * 0.5f, center - obstacleSize * 0.5f,
                   obstacleSize, obstacleSize, false});

  GameState game;
  game.carPosition = GameVec2{center, center};
  game.carSpeed = 0.0f;
  UpdateGame(&game, 0.0f, {}, 0.01f, &city);
  assert(game.hitObstacle);
  assert(!PointInRect(game.carPosition.x, game.carPosition.y,
                      center - obstacleSize * 0.5f, center - obstacleSize * 0.5f,
                      obstacleSize, obstacleSize));
}

void TestCityObstacleDoesNotBounceWhenEscaping() {
  CityMap city;
  city.columns = 2;
  city.rows = 2;
  const float center = kCityTileSize * 0.5f;
  const float obstacleSize = kCityTileSize * 0.35f;
  const float obstacleLeft = center - obstacleSize * 0.5f;
  city.obstacles.push_back(
      CityObstacle{obstacleLeft, center - obstacleSize * 0.5f, obstacleSize, obstacleSize, false});

  GameState game;
  game.carHeadingDeg = 90.0f;
  game.carPosition = GameVec2{obstacleLeft + obstacleSize + kGameCarCollisionRadius - 2.0f, center};
  game.carSpeed = 40.0f;
  UpdateGame(&game, 0.0f, {}, 0.01f, &city);
  assert(game.hitObstacle);
  assert(game.carSpeed > 0.0f);
}

void TestCityObstacleStopsWithoutReversingDirection() {
  CityMap city;
  city.columns = 2;
  city.rows = 2;
  const float center = kCityTileSize * 0.5f;
  const float obstacleSize = kCityTileSize * 0.35f;
  city.obstacles.push_back(
      CityObstacle{center - obstacleSize * 0.5f, center - obstacleSize * 0.5f,
                   obstacleSize, obstacleSize, false});

  GameState game;
  game.carPosition = GameVec2{center - obstacleSize * 0.5f - kGameCarCollisionRadius - 1.0f, center};
  game.carHeadingDeg = 90.0f;
  game.carSpeed = 80.0f;
  UpdateGame(&game, 0.0f, GameButtons{true, false}, 0.1f, &city);
  assert(game.hitObstacle);
  assert(game.carSpeed == 0.0f);
}

void TestSteeringSpeedFactorDropsAtSpeed() {
  assert(ComputeSteeringSpeedFactor(0.0f) > ComputeSteeringSpeedFactor(kGameMaxSpeed));
  assert(ComputeSteeringSpeedFactor(kGameMaxSpeed) >= 0.90f);
}

void TestManualModeUsesHigherTopSpeed() {
  GameState game;
  game.carSpeed = kGameManualMaxSpeed - 10.0f;
  UpdateGame(&game, 0.0f, GameButtons{true, false}, 1.0f);
  assert(game.carSpeed == kGameManualMaxSpeed);
}

void TestLowSpeedYawFactorNearZeroWhenStopped() {
  assert(ComputeLowSpeedYawFactor(0.0f) == 0.0f);
  assert(ComputeLowSpeedYawFactor(28.0f) >= 0.99f);
}

void TestReverseSteeringTurnsOppositeDirection() {
  GameState game;
  game.carSpeed = -50.0f;
  const float startHeading = game.carHeadingDeg;
  UpdateGame(&game, 1.0f, GameButtons{false, true}, 0.25f);
  assert(game.carHeadingDeg < startHeading);
}
}  // namespace

int main() {
  TestPacketParser();
  TestSensorReceiverKeepsLatestValidPacket();
  TestSensorReceiverReturnsFalseWhenOnlyInvalidPacketsArrive();
  TestCenteredPitchTwist();
  TestSteeringInputWraparound();
  TestKeyboardSteeringFallback();
  TestPauseMenuSteeringAndButtons();
  TestPauseChordOpensMenu();
  TestServerDiscoveryBeacon();
  TestAutoDriveAdvancesCar();
  TestManualThrottleAndBrake();
  TestManualBrakeCanReverse();
  TestManualBrakeStopsBeforeReverse();
  TestDriveModeToggle();
  TestCoinCollection();
  TestCitySpawnAndCoinCollection();
  TestCityKrakenCollection();
  TestCityRoadDetection();
  TestCityMapParserWarnings();
  TestRoadArtTuningWarningsAndClamps();
  TestCityObstaclePushesCar();
  TestCityObstacleDoesNotBounceWhenEscaping();
  TestCityObstacleStopsWithoutReversingDirection();
  TestSteeringSpeedFactorDropsAtSpeed();
  TestManualModeUsesHigherTopSpeed();
  TestLowSpeedYawFactorNearZeroWhenStopped();
  TestReverseSteeringTurnsOppositeDirection();
  return 0;
}
