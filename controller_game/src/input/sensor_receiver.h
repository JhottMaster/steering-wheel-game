#pragma once

#include <array>

#include "../game/game_logic.h"
#include "datagram_receive.h"

template <typename Receiver>
bool PollLatestSensorFrame(Receiver* receiver, SensorFrame* frame) {
  bool receivedFrame = false;
  std::array<char, 256> buffer{};

  while (true) {
    int receivedBytes = 0;
    const DatagramReceiveStatus status =
        receiver->ReceiveDatagram(buffer.data(), static_cast<int>(buffer.size()) - 1, &receivedBytes);

    if (status == DatagramReceiveStatus::kWouldBlock) {
      break;
    }
    if (status == DatagramReceiveStatus::kError) {
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
