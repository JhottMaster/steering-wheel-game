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
#include <cstdint>
#include <string>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../input/datagram_receive.h"
#include "raylib.h"

#pragma comment(lib, "ws2_32.lib")

namespace platform {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
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
  SetWindowMinSize(1024, 768);
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

  DatagramReceiveStatus ReceiveDatagram(char* buffer, int capacity, int* receivedBytes) {
    sockaddr_in sender{};
    int senderLength = sizeof(sender);
    const int bytes = recvfrom(socket, buffer, capacity, 0, reinterpret_cast<sockaddr*>(&sender),
                               &senderLength);
    if (bytes == SOCKET_ERROR) {
      const int error = WSAGetLastError();
      if (error == WSAEWOULDBLOCK) {
        return DatagramReceiveStatus::kWouldBlock;
      }
      return DatagramReceiveStatus::kError;
    }

    *receivedBytes = bytes;
    return DatagramReceiveStatus::kPacket;
  }

  ~UdpReceiver() { Close(); }
};

struct UdpBroadcaster {
  SocketHandle socket = kInvalidSocket;
  bool winsockStarted = false;

  bool Open() {
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

    BOOL enabled = TRUE;
    if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&enabled),
                   sizeof(enabled)) != 0) {
      Close();
      return false;
    }

    return true;
  }

  bool SendBroadcast(const char* payload, int payloadLength, int port) {
    if (socket == kInvalidSocket) {
      return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    address.sin_port = htons(static_cast<uint16_t>(port));

    return sendto(socket, payload, payloadLength, 0, reinterpret_cast<const sockaddr*>(&address),
                  sizeof(address)) == payloadLength;
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

  ~UdpBroadcaster() { Close(); }
};

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
