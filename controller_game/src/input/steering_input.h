#pragma once

#include <algorithm>
#include <cmath>

#include "../game/game_logic.h"

constexpr float kSteeringKeyboardFallbackFullLockDeg = 45.0f;
constexpr float kSteeringInputDeadzoneDeg = 2.0f;
constexpr float kSteeringInputDeadzoneBlendDeg = 8.0f;
constexpr float kSteeringInputEarlyResponseFraction = 0.85f;
constexpr float kSteeringInputEarlyRangeDeg = 90.0f;
constexpr float kSteeringInputFullLockDeg = 270.0f;
constexpr float kSteeringKeyboardResponsePerSecond = 220.0f;
constexpr float kSteeringKeyboardReturnPerSecond = 280.0f;

struct SteeringInputState {
  SensorFrame centerFrame;
  bool hasCenterFrame = false;
  float manualAngleDeg = 0.0f;
  float accumulatedGameAngleDeg = 0.0f;
  float lastWrappedGameAngleDeg = 0.0f;
  bool hasLastWrappedGameAngle = false;
};

inline float GetWrappedGameWheelAngleDeg(const SensorFrame& frame,
                                         const SteeringInputState& steering) {
  return steering.hasCenterFrame
             ? GetCenteredAxisDegrees(OrientationAxis::kPitch, frame, steering.centerFrame)
             : GetAxisDegrees(OrientationAxis::kPitch, frame);
}

inline void ResetControllerCenter(const SensorFrame& lastGoodFrame, bool hasAnyPacket,
                                  SteeringInputState* steering) {
  if (hasAnyPacket) {
    steering->centerFrame = lastGoodFrame;
    steering->hasCenterFrame = true;
  } else {
    steering->manualAngleDeg = 0.0f;
    steering->hasCenterFrame = false;
  }
  steering->accumulatedGameAngleDeg = 0.0f;
  steering->lastWrappedGameAngleDeg = 0.0f;
  steering->hasLastWrappedGameAngle = false;
}

inline void RecordSensorFrameForSteering(const SensorFrame& frame, SteeringInputState* steering) {
  if (!steering->hasCenterFrame) {
    steering->centerFrame = frame;
    steering->hasCenterFrame = true;
  }

  const float wrappedGameAngleDeg = GetWrappedGameWheelAngleDeg(frame, *steering);
  if (!steering->hasLastWrappedGameAngle) {
    steering->accumulatedGameAngleDeg = wrappedGameAngleDeg;
    steering->hasLastWrappedGameAngle = true;
  } else {
    steering->accumulatedGameAngleDeg +=
        WrapDegrees(wrappedGameAngleDeg - steering->lastWrappedGameAngleDeg);
  }
  steering->lastWrappedGameAngleDeg = wrappedGameAngleDeg;
}

inline void UpdateKeyboardSteeringFallback(SteeringInputState* steering, float keyboardDirection,
                                           bool hasFreshPackets, float dt) {
  if (hasFreshPackets) {
    steering->manualAngleDeg = 0.0f;
    return;
  }

  const float targetManualAngleDeg = keyboardDirection * kSteeringKeyboardFallbackFullLockDeg;
  const float responsePerSecond = keyboardDirection == 0.0f
                                      ? kSteeringKeyboardReturnPerSecond
                                      : kSteeringKeyboardResponsePerSecond;
  const float maxStepDeg = responsePerSecond * dt;
  if (steering->manualAngleDeg < targetManualAngleDeg) {
    steering->manualAngleDeg =
        std::min(steering->manualAngleDeg + maxStepDeg, targetManualAngleDeg);
  } else if (steering->manualAngleDeg > targetManualAngleDeg) {
    steering->manualAngleDeg =
        std::max(steering->manualAngleDeg - maxStepDeg, targetManualAngleDeg);
  }
}

inline float ApplyGameSteeringResponseCurve(float wheelAngleDeg) {
  const float sign = wheelAngleDeg < 0.0f ? -1.0f : 1.0f;
  const float absoluteAngleDeg = std::fabs(wheelAngleDeg);
  if (absoluteAngleDeg <= kSteeringInputDeadzoneDeg) {
    return 0.0f;
  }

  const float deadzoneBlendEndDeg = kSteeringInputDeadzoneDeg + kSteeringInputDeadzoneBlendDeg;
  float softenedAngleDeg = absoluteAngleDeg;
  if (absoluteAngleDeg < deadzoneBlendEndDeg) {
    const float t = (absoluteAngleDeg - kSteeringInputDeadzoneDeg) /
                    kSteeringInputDeadzoneBlendDeg;
    const float smoothedT = t * t * (3.0f - 2.0f * t);
    softenedAngleDeg = smoothedT * deadzoneBlendEndDeg;
  }

  const float clampedAngleDeg = std::min(softenedAngleDeg, kSteeringInputFullLockDeg);

  if (clampedAngleDeg <= kSteeringInputEarlyRangeDeg) {
    const float earlyFraction = clampedAngleDeg / kSteeringInputEarlyRangeDeg;
    return sign * earlyFraction * kSteeringInputEarlyResponseFraction;
  }

  const float remainingAngleDeg = kSteeringInputFullLockDeg - kSteeringInputEarlyRangeDeg;
  const float trailingFraction =
      remainingAngleDeg <= 0.0f ? 1.0f
                                : (clampedAngleDeg - kSteeringInputEarlyRangeDeg) /
                                      remainingAngleDeg;
  const float normalized =
      kSteeringInputEarlyResponseFraction +
      trailingFraction * (1.0f - kSteeringInputEarlyResponseFraction);
  return sign * normalized;
}
