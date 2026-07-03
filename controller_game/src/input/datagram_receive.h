#pragma once

enum class DatagramReceiveStatus {
  kPacket,
  kWouldBlock,
  kError,
};
