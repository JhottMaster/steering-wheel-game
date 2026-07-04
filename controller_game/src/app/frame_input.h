#pragma once

#include <chrono>

#include "app_runtime.h"
#include "controller_buttons.h"
#include "../input/sensor_receiver.h"
#include "raylib.h"

constexpr float kVisibleAngleRangeDeg = 45.0f;
constexpr float kPacketTimeoutSeconds = 1.0f;
constexpr float kDisplaySteeringDirection = -1.0f;
constexpr float kGameSteeringDirection = -1.0f;

struct FrameInput {
  float dt = 0.0f;
  std::chrono::steady_clock::time_point now = {};
  bool hasFreshPackets = false;
  bool hasAnyPacket = false;
  bool roadArtEditorConsumesArrows = false;
  ControllerButtonState menuButtons;
  bool pauseChordDown = false;
  float pauseMenuSteeringAngleDeg = 0.0f;
  float sourceAngleDeg = 0.0f;
  float centeredAngleDeg = 0.0f;
  float steeringAngleDeg = 0.0f;
  float normalizedValue = 0.0f;
  float gameSteeringInput = 0.0f;
  GameButtons gameButtons;
};

template <typename Receiver>
FrameInput ReadFrameInput(AppRuntime* app, Receiver* receiver) {
  if (PollLatestSensorFrame(receiver, &app->latestFrame)) {
    app->lastGoodFrame = app->latestFrame;
    app->lastPacketTime = std::chrono::steady_clock::now();
    MarkControllerPacketReceived(&app->discovery);
    RecordSensorFrameForSteering(app->lastGoodFrame, &app->steeringInput);
  }

  FrameInput input;
  input.dt = GetFrameTime();
  input.now = std::chrono::steady_clock::now();

  const float secondsSincePacket =
      std::chrono::duration<float>(input.now - app->lastPacketTime).count();
  input.hasFreshPackets = secondsSincePacket <= kPacketTimeoutSeconds;
  input.hasAnyPacket = app->lastPacketTime != std::chrono::steady_clock::time_point{};
  input.roadArtEditorConsumesArrows =
      app->appMode == AppMode::kGame && app->roadArtEditor.active;

  const float rawKeyboardDirection =
      input.roadArtEditorConsumesArrows
          ? 0.0f
          : static_cast<float>(IsKeyDown(KEY_LEFT)) - static_cast<float>(IsKeyDown(KEY_RIGHT));
  const float keyboardTravelDirection = app->game.carSpeed < 0.0f ? -1.0f : 1.0f;
  UpdateKeyboardSteeringFallback(&app->steeringInput,
                                 rawKeyboardDirection * keyboardTravelDirection,
                                 input.hasFreshPackets, input.dt);

  input.menuButtons = ReadControllerButtons(app->lastGoodFrame, input.hasFreshPackets);
  input.pauseChordDown = input.menuButtons.green && input.menuButtons.red;
  input.pauseMenuSteeringAngleDeg =
      input.hasAnyPacket ? app->steeringInput.accumulatedGameAngleDeg * kGameSteeringDirection
                         : app->steeringInput.manualAngleDeg;

  input.sourceAngleDeg = input.hasAnyPacket ? GetAxisDegrees(app->displayAxis, app->lastGoodFrame)
                                            : app->steeringInput.manualAngleDeg;
  input.centeredAngleDeg =
      input.hasAnyPacket && app->steeringInput.hasCenterFrame
          ? GetCenteredAxisDegrees(app->displayAxis, app->lastGoodFrame,
                                   app->steeringInput.centerFrame)
          : input.sourceAngleDeg;
  input.steeringAngleDeg = input.centeredAngleDeg * kDisplaySteeringDirection;
  input.normalizedValue = ClampUnit(input.steeringAngleDeg / kVisibleAngleRangeDeg);

  const float gameSourceAngleDeg =
      input.hasAnyPacket ? app->steeringInput.accumulatedGameAngleDeg
                         : app->steeringInput.manualAngleDeg;
  input.gameSteeringInput =
      ClampUnit(ApplyGameSteeringResponseCurve(gameSourceAngleDeg * kGameSteeringDirection));

  const ControllerButtonState gameInputButtons =
      ReadControllerButtons(app->lastGoodFrame, input.hasFreshPackets,
                            !input.roadArtEditorConsumesArrows);
  input.gameButtons = GameButtons{gameInputButtons.green, gameInputButtons.red};
  return input;
}
