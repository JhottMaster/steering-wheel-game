#pragma once

#include <string>

#include "app_runtime.h"
#include "frame_input.h"
#include "../game/game_view.h"
#include "../views/center_confirm_view.h"
#include "../views/hardware_test_view.h"
#include "../views/pause_menu_view.h"
#include "raylib.h"

inline void DrawPauseOverlay(const AppRuntime& app, const FrameInput& input,
                             const std::string& localIpText, int screenWidth,
                             int screenHeight) {
  if (!app.pauseMenu.active) {
    return;
  }

  if (app.centerConfirm.active) {
    DrawCenterConfirm(app.centerConfirm, input.steeringAngleDeg, input.hasFreshPackets,
                      screenWidth, screenHeight);
    return;
  }

  DrawPauseMenu(app.pauseMenu, app.game, app.appMode, localIpText, app.cameraZoomScale,
                screenWidth, screenHeight);
}

inline void DrawApp(const AppRuntime& app, const FrameInput& input, bool udpReady,
                    const std::string& localIpText) {
  const int screenWidth = GetScreenWidth();
  const int screenHeight = GetScreenHeight();

  BeginDrawing();
  if (app.appMode == AppMode::kGame) {
    DrawGame(app.game, app.gameAssets, app.city, app.roadArtTuning, app.roadArtEditor,
             input.hasFreshPackets, input.hasAnyPacket, app.discovery.wasBroadcastingForLoss,
             screenWidth, screenHeight, app.cameraZoomScale);
  } else {
    DrawHardwareTest(app.lastGoodFrame, app.displayAxis, input.sourceAngleDeg,
                     input.centeredAngleDeg, input.steeringAngleDeg, input.normalizedValue,
                     input.menuButtons, input.hasFreshPackets, input.hasAnyPacket, udpReady,
                     localIpText, app.steeringWheel3D, screenWidth, screenHeight);
  }

  DrawPauseOverlay(app, input, localIpText, screenWidth, screenHeight);
  EndDrawing();
}
