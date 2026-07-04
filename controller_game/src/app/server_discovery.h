#pragma once

#include <chrono>
#include <cstdio>

constexpr int kServerDiscoveryPort = 4211;
constexpr float kServerBeaconIntervalSeconds = 3.0f;
constexpr float kServerBeaconStartDelaySeconds = 5.0f;
constexpr char kServerDiscoveryMessage[] = "steering-wheel-server port=4210";

struct DiscoveryBeaconState {
  std::chrono::steady_clock::time_point lastBeaconTime = {};
  bool hasEverReceivedPacket = false;
  bool wasBroadcastingForLoss = false;
};

inline void MarkControllerPacketReceived(DiscoveryBeaconState* discovery) {
  discovery->hasEverReceivedPacket = true;
  discovery->wasBroadcastingForLoss = false;
}

template <typename Broadcaster>
void UpdateServerDiscoveryBeacon(DiscoveryBeaconState* discovery, Broadcaster* broadcaster,
                                 bool broadcastReady, bool hasFreshPackets,
                                 std::chrono::steady_clock::time_point lastPacketTime,
                                 std::chrono::steady_clock::time_point now,
                                 bool logBroadcast = true) {
  const float secondsSinceBeacon =
      discovery->lastBeaconTime == std::chrono::steady_clock::time_point{}
          ? kServerBeaconIntervalSeconds
          : std::chrono::duration<float>(now - discovery->lastBeaconTime).count();

  const bool shouldStartInitialBroadcast = !discovery->hasEverReceivedPacket;
  const bool shouldStartLossBroadcast =
      discovery->hasEverReceivedPacket && !hasFreshPackets &&
      std::chrono::duration<float>(now - lastPacketTime).count() >=
          kServerBeaconStartDelaySeconds;
  const bool shouldBroadcast = shouldStartInitialBroadcast || shouldStartLossBroadcast;

  if (shouldStartLossBroadcast && !discovery->wasBroadcastingForLoss) {
    discovery->lastBeaconTime = std::chrono::steady_clock::time_point{};
    discovery->wasBroadcastingForLoss = true;
  }

  if (broadcastReady && shouldBroadcast && secondsSinceBeacon >= kServerBeaconIntervalSeconds) {
    broadcaster->SendBroadcast(kServerDiscoveryMessage,
                               static_cast<int>(sizeof(kServerDiscoveryMessage) - 1),
                               kServerDiscoveryPort);
    if (logBroadcast) {
      std::printf("Server discovery broadcast sent to UDP %d: %s\n", kServerDiscoveryPort,
                  kServerDiscoveryMessage);
    }
    discovery->lastBeaconTime = now;
  }
}
