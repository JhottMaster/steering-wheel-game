#pragma once

#include <string>

#include "../app/pause_menu.h"
#include "../game/game_logic.h"
#include "raylib.h"

inline void DrawOutlinedRoundedRect(Rectangle rect, float roundness, int segments, float thickness,
                                    Color fill, Color outline) {
  DrawRectangleRounded(rect, roundness, segments, outline);
  Rectangle inner = {rect.x + thickness, rect.y + thickness, rect.width - thickness * 2.0f,
                     rect.height - thickness * 2.0f};
  DrawRectangleRounded(inner, roundness, segments, fill);
}

inline void DrawPauseMenu(const PauseMenuState& menu, const GameState& game, AppMode appMode,
                          const std::string& localIpText, float cameraZoomScale,
                          int screenWidth, int screenHeight) {
  DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 115});

  const float panelWidth = std::min(620.0f, static_cast<float>(screenWidth) - 80.0f);
  const float panelHeight = std::min(610.0f, static_cast<float>(screenHeight) - 70.0f);
  const float panelX = (screenWidth - panelWidth) * 0.5f;
  const float panelY = (screenHeight - panelHeight) * 0.5f;
  Rectangle panel = {panelX, panelY, panelWidth, panelHeight};
  DrawOutlinedRoundedRect(panel, 0.08f, 16, 3.0f, Color{252, 244, 222, 238},
                          Color{73, 49, 31, 230});

  DrawText("Paused", static_cast<int>(panelX + 34.0f), static_cast<int>(panelY + 26.0f), 42,
           Color{54, 36, 23, 255});
  DrawText("Steer to choose  |  Green selects  |  Red goes back",
           static_cast<int>(panelX + 36.0f), static_cast<int>(panelY + 78.0f), 20,
           Color{92, 69, 49, 220});

  const int columns = 2;
  const float itemWidth = (panelWidth - 92.0f) / columns;
  const float itemHeight = 74.0f;
  const float startX = panelX + 34.0f;
  const float startY = panelY + 132.0f;
  const float gapX = 24.0f;
  const float gapY = 16.0f;

  for (int i = 0; i < static_cast<int>(kPauseMenuItems.size()); ++i) {
    const int column = i % columns;
    const int row = i / columns;
    const float x = startX + column * (itemWidth + gapX);
    const float y = startY + row * (itemHeight + gapY);
    const bool selected = i == menu.selectedIndex;
    const Color fill = selected ? Color{233, 104, 66, 245} : Color{255, 252, 240, 245};
    const Color outline = selected ? Color{100, 38, 22, 255} : Color{138, 104, 70, 190};
    const Color textColor = selected ? RAYWHITE : Color{60, 42, 29, 255};
    Rectangle itemRect = {x, y, itemWidth, itemHeight};
    DrawOutlinedRoundedRect(itemRect, 0.18f, 12, selected ? 4.0f : 2.0f, fill, outline);

    const char* label = GetPauseMenuItemLabel(kPauseMenuItems[static_cast<size_t>(i)], game,
                                              appMode);
    const int fontSize = 24;
    const int textWidth = MeasureText(label, fontSize);
    DrawText(label, static_cast<int>(x + (itemWidth - textWidth) * 0.5f),
             static_cast<int>(y + (itemHeight - fontSize) * 0.5f), fontSize, textColor);
  }

  const float diagnosticsY = panelY + panelHeight - 64.0f;
  DrawRectangleRounded(Rectangle{panelX + 34.0f, diagnosticsY, panelWidth - 68.0f, 38.0f},
                       0.20f, 10, Color{84, 61, 42, 45});
  DrawText(TextFormat("FPS: %d   zoom: %.2fx", GetFPS(), cameraZoomScale),
           static_cast<int>(panelX + 52.0f), static_cast<int>(diagnosticsY + 10.0f), 18,
           Color{92, 69, 49, 230});
  const int ipTextWidth = MeasureText(localIpText.c_str(), 16);
  DrawText(localIpText.c_str(),
           static_cast<int>(panelX + panelWidth - 52.0f - ipTextWidth),
           static_cast<int>(diagnosticsY + 12.0f), 16, Color{92, 69, 49, 200});
}
