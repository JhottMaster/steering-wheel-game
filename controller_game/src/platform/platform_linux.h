#pragma once

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../input/datagram_receive.h"
#include "raylib.h"

namespace platform {
#if defined(STEERING_WHEEL_RASPBERRY_PI)
constexpr int kWindowWidth = 1920;
constexpr int kWindowHeight = 1080;
constexpr bool kConsoleBuild = true;
constexpr const char* kWindowTitle = "Toddler Steering Wheel Console";
constexpr const char* kHeaderTitle = "Toddler Steering Wheel Console";
constexpr const char* kHeaderHelp =
    "T: game/test   A: drive mode   P/R/Y: axis   SPACE: center   ESC: quit";
constexpr const char* kGameHelp = "T: hardware test   ESC: quit";
#else
constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;
constexpr bool kConsoleBuild = false;
constexpr const char* kWindowTitle = "Steering Wheel Controller Game";
constexpr const char* kHeaderTitle = "Toddler Steering Wheel Game";
constexpr const char* kHeaderHelp =
    "T: game/test   F11: fullscreen   P/R/Y: axis   SPACE: center   A/D or arrows: fallback input";
constexpr const char* kGameHelp = "T: hardware test   F11: fullscreen";
#endif

using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

inline unsigned int GetWindowConfigFlags() {
  unsigned int flags = FLAG_VSYNC_HINT;
  if (!kConsoleBuild) {
    flags |= FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT;
  }
  return flags;
}

inline void ApplyPostWindowInit() {
  if (kConsoleBuild) {
    HideCursor();
    return;
  }

  SetWindowMinSize(960, 540);
  MaximizeWindow();
}

inline bool ShouldToggleFullscreen() {
  return !kConsoleBuild && IsKeyPressed(KEY_F11);
}

inline void DrawRoundedRectangleLines(Rectangle bounds, float roundness, int segments,
                                      float lineThick, Color color) {
  (void)lineThick;
  DrawRectangleRoundedLines(bounds, roundness, segments, color);
}

struct UdpReceiver {
  SocketHandle socket = kInvalidSocket;

  bool Open(int port) {
    socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == kInvalidSocket) {
      Close();
      return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
      Close();
      return false;
    }

    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0 || fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
      Close();
      return false;
    }

    return true;
  }

  void Close() {
    if (socket != kInvalidSocket) {
      close(socket);
      socket = kInvalidSocket;
    }
  }

  DatagramReceiveStatus ReceiveDatagram(char* buffer, int capacity, int* receivedBytes) {
    sockaddr_in sender{};
    socklen_t senderLength = sizeof(sender);
    const int bytes =
        recvfrom(socket, buffer, static_cast<size_t>(capacity), 0,
                 reinterpret_cast<sockaddr*>(&sender), &senderLength);
    if (bytes < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        return DatagramReceiveStatus::kWouldBlock;
      }
      return DatagramReceiveStatus::kError;
    }

    *receivedBytes = bytes;
    return DatagramReceiveStatus::kPacket;
  }

  ~UdpReceiver() { Close(); }
};

inline std::vector<std::string> GetLocalIpv4Addresses() {
  std::vector<std::string> addresses;

  ifaddrs* interfaces = nullptr;
  if (getifaddrs(&interfaces) == 0) {
    for (ifaddrs* iface = interfaces; iface != nullptr; iface = iface->ifa_next) {
      if (iface->ifa_addr == nullptr || iface->ifa_addr->sa_family != AF_INET) {
        continue;
      }
      if ((iface->ifa_flags & IFF_UP) == 0 || (iface->ifa_flags & IFF_LOOPBACK) != 0) {
        continue;
      }

      char ipBuffer[INET_ADDRSTRLEN] = {};
      const sockaddr_in* ipv4 = reinterpret_cast<const sockaddr_in*>(iface->ifa_addr);
      if (inet_ntop(AF_INET, &(ipv4->sin_addr), ipBuffer, sizeof(ipBuffer)) != nullptr) {
        std::string address(ipBuffer);
        if (std::find(addresses.begin(), addresses.end(), address) == addresses.end()) {
          addresses.push_back(address);
        }
      }
    }
    freeifaddrs(interfaces);
  }

  if (addresses.empty()) {
    addresses.emplace_back("No active IPv4 address found");
  }

  return addresses;
}
}  // namespace platform
