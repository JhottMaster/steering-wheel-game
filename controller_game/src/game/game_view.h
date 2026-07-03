#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#if defined(_WIN32)
#include "../platform/platform_windows.h"
#else
#include "../platform/platform_linux.h"
#endif

#include "game_assets.h"
#include "game_logic.h"
#include "raylib.h"

namespace game_view_detail {
constexpr float kDegToRad = 0.017453292519943295769f;

inline Vector2 PointOnCircle(Vector2 center, float radius, float angleDeg) {
  const float radians = angleDeg * kDegToRad;
  return Vector2{center.x + std::cos(radians) * radius,
                 center.y + std::sin(radians) * radius};
}

inline Vector2 ToVector2(GameVec2 value) {
  return Vector2{value.x, value.y};
}

inline Camera2D BuildGameCamera(const GameState& game, int screenWidth, int screenHeight,
                                float mapWidth, float mapHeight) {
  constexpr float targetWorldWidth = 640.0f;
  constexpr float minZoom = 1.0f;
  constexpr float maxZoom = 2.4f;
  const float zoom =
      std::clamp(static_cast<float>(screenWidth) / targetWorldWidth, minZoom, maxZoom);
  const float halfViewWidth = static_cast<float>(screenWidth) * 0.5f / zoom;
  const float halfViewHeight = static_cast<float>(screenHeight) * 0.5f / zoom;
  Camera2D camera = {};
  camera.offset =
      Vector2{static_cast<float>(screenWidth) * 0.5f, static_cast<float>(screenHeight) * 0.5f};
  if (halfViewWidth * 2.0f >= mapWidth) {
    camera.target.x = mapWidth * 0.5f;
  } else {
    camera.target.x = std::clamp(game.carPosition.x, halfViewWidth, mapWidth - halfViewWidth);
  }
  if (halfViewHeight * 2.0f >= mapHeight) {
    camera.target.y = mapHeight * 0.5f;
  } else {
    camera.target.y = std::clamp(game.carPosition.y, halfViewHeight, mapHeight - halfViewHeight);
  }
  camera.rotation = 0.0f;
  camera.zoom = zoom;
  return camera;
}

inline void DrawTextureCover(Texture2D texture, Rectangle destination, float rotationDeg = 0.0f) {
  if (!TextureLoaded(texture)) {
    return;
  }

  const Rectangle source = {0.0f, 0.0f, static_cast<float>(texture.width),
                            static_cast<float>(texture.height)};
  DrawTexturePro(texture, source, destination, Vector2{0.0f, 0.0f}, rotationDeg, WHITE);
}

inline void DrawCityTerrain(const GameAssets& assets, const CityMap& city) {
  for (int row = 0; row < city.rows; ++row) {
    for (int column = 0; column < city.columns; ++column) {
      const Rectangle destination = {column * kCityTileSize, row * kCityTileSize, kCityTileSize,
                                     kCityTileSize};
      if (TextureLoaded(assets.terrain)) {
        DrawTextureCover(assets.terrain, destination);
      } else {
        DrawRectangleRec(destination, Color{68, 142, 134, 255});
      }
    }
  }
}

inline void DrawCityVisuals(const GameAssets& assets, const CityMap& city) {
  for (const CityVisual& visual : city.visuals) {
    const Texture2D texture = GetCityTexture(assets, visual.sprite);
    const Rectangle destination = {visual.x, visual.y, visual.width, visual.height};
    if (TextureLoaded(texture)) {
      DrawTextureCover(texture, destination, visual.rotationDeg);
    } else {
      DrawRectangleLinesEx(destination, 3.0f, Color{255, 80, 80, 255});
    }
  }
}
}  // namespace game_view_detail

inline void DrawGame(const GameState& game, const GameAssets& assets, const CityMap& city,
                     bool hasFreshPackets, bool hasAnyPacket, bool isBroadcastingForLoss,
                     const std::string& localIpText, int screenWidth, int screenHeight) {
  ClearBackground(Color{60, 133, 126, 255});

  const bool hasCity = city.columns > 0 && city.rows > 0;
  const float mapWidth = hasCity ? city.columns * kCityTileSize : kGameMapSize;
  const float mapHeight = hasCity ? city.rows * kCityTileSize : kGameMapSize;
  const Camera2D camera =
      game_view_detail::BuildGameCamera(game, screenWidth, screenHeight, mapWidth, mapHeight);
  BeginMode2D(camera);
  if (hasCity) {
    game_view_detail::DrawCityTerrain(assets, city);
    game_view_detail::DrawCityVisuals(assets, city);
  } else if (TextureLoaded(assets.terrain)) {
    const Rectangle destination = {0.0f, 0.0f, kGameMapSize, kGameMapSize};
    game_view_detail::DrawTextureCover(assets.terrain, destination);
  } else {
    DrawRectangle(0, 0, static_cast<int>(kGameMapSize), static_cast<int>(kGameMapSize),
                  Color{68, 142, 134, 255});
  }

  if (!hasCity) {
    for (const CoinState& coin : game.coins) {
      if (coin.collected) {
        continue;
      }
      if (TextureLoaded(assets.coin)) {
        const Rectangle source = {0.0f, 0.0f, static_cast<float>(assets.coin.width),
                                  static_cast<float>(assets.coin.height)};
        const Rectangle destination = {coin.position.x, coin.position.y, 46.0f, 46.0f};
        DrawTexturePro(assets.coin, source, destination, Vector2{23.0f, 23.0f}, 0.0f, WHITE);
      } else {
        DrawCircleV(game_view_detail::ToVector2(coin.position), 22.0f, GOLD);
      }
    }
  }

  if (TextureLoaded(assets.car)) {
    const Rectangle source = {0.0f, 0.0f, static_cast<float>(assets.car.width),
                              static_cast<float>(assets.car.height)};
    const float carDrawWidth = 58.0f;
    const float carDrawHeight =
        carDrawWidth * static_cast<float>(assets.car.height) / static_cast<float>(assets.car.width);
    const Rectangle destination = {game.carPosition.x, game.carPosition.y, carDrawWidth,
                                   carDrawHeight};
    DrawTexturePro(assets.car, source, destination,
                   Vector2{carDrawWidth * 0.5f, carDrawHeight * 0.5f}, game.carHeadingDeg, WHITE);
  } else {
    const Vector2 nose =
        game_view_detail::PointOnCircle(game_view_detail::ToVector2(game.carPosition), 34.0f,
                                        game.carHeadingDeg - 90.0f);
    DrawCircleV(game_view_detail::ToVector2(game.carPosition), 28.0f, RED);
    DrawCircleV(nose, 8.0f, YELLOW);
  }
  EndMode2D();

  const int leftPanelWidth = platform::kConsoleBuild ? 480 : 420;
  const int rightPanelX = screenWidth - (platform::kConsoleBuild ? 500 : 430);
  const int rightPanelWidth = platform::kConsoleBuild ? 476 : 410;
  const int rightTextX = screenWidth - (platform::kConsoleBuild ? 476 : 408);

  DrawRectangle(20, 20, leftPanelWidth, 122, Color{18, 28, 32, 185});
  DrawText("Road Carpet Drive", 38, 36, 30, Color{255, 244, 205, 255});
  if (hasCity) {
    DrawText(TextFormat("city: %d x %d tiles", city.columns, city.rows), 40, 74, 22,
             Color{232, 236, 224, 255});
  } else {
    DrawText(TextFormat("coins: %d / %d", game.score, static_cast<int>(game.coins.size())), 40, 74,
             22, Color{232, 236, 224, 255});
  }
  DrawText(TextFormat("drive: %s   speed: %.0f",
                      game.driveMode == DriveMode::kAuto ? "auto" : "button", game.carSpeed),
           40, 104, 20, Color{232, 236, 224, 255});

  DrawRectangle(rightPanelX, 20, rightPanelWidth, 146, Color{18, 28, 32, 185});
  DrawText(platform::kGameHelp, rightTextX, 36, 22, Color{232, 236, 224, 255});
  DrawText("A: auto/button drive   SPACE: center", rightTextX, 66, 19,
           Color{195, 214, 204, 255});
  DrawText(hasFreshPackets ? "UDP controller active" : "keyboard fallback", rightTextX, 94, 19,
           hasFreshPackets ? Color{104, 230, 141, 255} : Color{246, 187, 87, 255});
  DrawText(TextFormat("FPS: %d", GetFPS()), rightTextX, 116, 19, Color{255, 244, 205, 255});
  DrawText(localIpText.c_str(), rightTextX, 140, 16, Color{176, 196, 186, 255});

  if (hasAnyPacket && !hasFreshPackets) {
    DrawRectangle(0, 0, screenWidth, 38, Color{153, 48, 32, 220});
    DrawText(isBroadcastingForLoss ? "Controller connection lost - rediscovering..."
                                   : "Controller connection lost",
             20, 9, 20, Color{255, 240, 232, 255});
  }
}
