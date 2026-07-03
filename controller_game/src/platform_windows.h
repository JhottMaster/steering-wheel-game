#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <string>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "game_logic.h"
#include "raylib.h"

#pragma comment(lib, "ws2_32.lib")

namespace platform {
constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;
constexpr bool kConsoleBuild = false;
constexpr const char* kWindowTitle = "Steering Wheel Controller Game";
constexpr const char* kHeaderTitle = "Toddler Steering Wheel Game";
constexpr const char* kHeaderHelp =
    "T: game/test   F11: fullscreen   P/R/Y: axis   SPACE: center   A/D or arrows: fallback input";
constexpr const char* kGameHelp = "T: hardware test   F11: fullscreen";

using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

inline unsigned int GetWindowConfigFlags() {
  return FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI;
}

inline void ApplyPostWindowInit() {
  SetWindowMinSize(960, 540);
  MaximizeWindow();
}

inline bool ShouldToggleFullscreen() {
  return IsKeyPressed(KEY_F11);
}

inline void DrawRoundedRectangleLines(Rectangle bounds, float roundness, int segments,
                                      float lineThick, Color color) {
  DrawRectangleRoundedLines(bounds, roundness, segments, lineThick, color);
}

struct UdpReceiver {
  SocketHandle socket = kInvalidSocket;
  bool winsockStarted = false;

  bool Open(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
      return false;
    }
    winsockStarted = true;

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

    u_long nonBlocking = 1;
    if (ioctlsocket(socket, FIONBIO, &nonBlocking) != 0) {
      Close();
      return false;
    }

    return true;
  }

  void Close() {
    if (socket != kInvalidSocket) {
      closesocket(socket);
      socket = kInvalidSocket;
    }

    if (winsockStarted) {
      WSACleanup();
      winsockStarted = false;
    }
  }

  ~UdpReceiver() { Close(); }
};

inline bool PollLatestSensorFrame(UdpReceiver* receiver, SensorFrame* frame) {
  bool receivedFrame = false;
  std::array<char, 256> buffer{};

  while (true) {
    sockaddr_in sender{};
    int senderLength = sizeof(sender);
    const int receivedBytes =
        recvfrom(receiver->socket, buffer.data(), static_cast<int>(buffer.size()) - 1, 0,
                 reinterpret_cast<sockaddr*>(&sender), &senderLength);
    if (receivedBytes == SOCKET_ERROR) {
      const int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK) {
        break;
      }
      return receivedFrame;
    }

    buffer[receivedBytes] = '\0';
    SensorFrame parsedFrame;
    if (ParsePacket(buffer.data(), &parsedFrame)) {
      *frame = parsedFrame;
      receivedFrame = true;
    }
  }

  return receivedFrame;
}

inline std::vector<std::string> GetLocalIpv4Addresses() {
  std::vector<std::string> addresses;
  char hostName[256] = {};
  if (gethostname(hostName, sizeof(hostName)) == 0) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* info = nullptr;
    if (getaddrinfo(hostName, nullptr, &hints, &info) == 0) {
      for (addrinfo* current = info; current != nullptr; current = current->ai_next) {
        const sockaddr_in* ipv4 = reinterpret_cast<const sockaddr_in*>(current->ai_addr);
        char ipBuffer[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &(ipv4->sin_addr), ipBuffer, sizeof(ipBuffer)) != nullptr) {
          std::string address(ipBuffer);
          if (address != "127.0.0.1" &&
              std::find(addresses.begin(), addresses.end(), address) == addresses.end()) {
            addresses.push_back(address);
          }
        }
      }
      freeaddrinfo(info);
    }
  }

  if (addresses.empty()) {
    addresses.emplace_back("No active IPv4 address found");
  }

  return addresses;
}
}  // namespace platform
