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
#include "road_art_tuning.h"
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
                                float mapWidth, float mapHeight, float zoomScale) {
  constexpr float targetWorldWidth = 640.0f;
  constexpr float minZoom = 0.18f;
  constexpr float maxZoom = 2.4f;
  const float baseZoom = static_cast<float>(screenWidth) / targetWorldWidth;
  const float zoom = std::clamp(baseZoom * zoomScale, minZoom, maxZoom);
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

inline Rectangle ScaleRectangleAnchored(Rectangle rectangle, float scale, int anchorX,
                                        int anchorY) {
  const float width = rectangle.width * scale;
  const float height = rectangle.height * scale;
  const float x =
      rectangle.x + (rectangle.width - width) * (static_cast<float>(anchorX) + 1.0f) * 0.5f;
  const float y =
      rectangle.y + (rectangle.height - height) * (static_cast<float>(anchorY) + 1.0f) * 0.5f;
  return Rectangle{x, y, width, height};
}

inline Rectangle OffsetRectangle(Rectangle rectangle, float offsetX, float offsetY) {
  rectangle.x += offsetX;
  rectangle.y += offsetY;
  return rectangle;
}

inline bool IsRoadCurveSprite(CitySprite sprite) {
  return sprite == CitySprite::kRoadCurveBottomRight ||
         sprite == CitySprite::kRoadCurveBottomLeft ||
         sprite == CitySprite::kRoadCurveTopRight ||
         sprite == CitySprite::kRoadCurveTopLeft;
}

inline bool IsRoadSprite(CitySprite sprite) {
  return sprite == CitySprite::kRoadHorizontal || sprite == CitySprite::kRoadVertical ||
         sprite == CitySprite::kRoadIntersection || IsRoadCurveSprite(sprite);
}

inline Vector2 RotateVector(Vector2 value, float angleDeg) {
  const float radians = angleDeg * kDegToRad;
  const float cosAngle = std::cos(radians);
  const float sinAngle = std::sin(radians);
  return Vector2{value.x * cosAngle - value.y * sinAngle,
                 value.x * sinAngle + value.y * cosAngle};
}

inline void DrawCarFrontWheels(const GameAssets& assets, const GameState& game,
                               float carDrawWidth, float carDrawHeight) {
  if (!TextureLoaded(assets.carTire)) {
    return;
  }

  const Rectangle source = {0.0f, 0.0f, static_cast<float>(assets.carTire.width),
                            static_cast<float>(assets.carTire.height)};
  const float tireWidth = carDrawWidth * 0.15f;
  const float tireHeight =
      tireWidth * static_cast<float>(assets.carTire.height) /
      static_cast<float>(assets.carTire.width);
  const Vector2 forward = RotateVector(Vector2{0.0f, -1.0f}, game.carHeadingDeg);
  const Vector2 right = RotateVector(Vector2{1.0f, 0.0f}, game.carHeadingDeg);
  const float frontOffset = carDrawHeight * 0.27f;
  const float sideOffset = carDrawWidth * 0.39f;
  const float wheelRotation = game.carHeadingDeg + game.visualWheelTurnDeg;

  for (const float side : {-1.0f, 1.0f}) {
    const Vector2 wheelCenter = {
        game.carPosition.x + forward.x * frontOffset + right.x * sideOffset * side,
        game.carPosition.y + forward.y * frontOffset + right.y * sideOffset * side,
    };
    const Rectangle destination = {wheelCenter.x, wheelCenter.y, tireWidth, tireHeight};
    DrawTexturePro(assets.carTire, source, destination,
                   Vector2{tireWidth * 0.5f, tireHeight * 0.5f}, wheelRotation, WHITE);
  }
}

inline void DrawTextureRepeatVerticalCentered(Texture2D texture, Rectangle destination,
                                              float drawWidth, float offsetX = 0.0f) {
  if (!TextureLoaded(texture) || texture.height <= 0 || texture.width <= 0) {
    return;
  }

  const float drawHeight =
      drawWidth * static_cast<float>(texture.height) / static_cast<float>(texture.width);
  const float x = destination.x + (destination.width - drawWidth) * 0.5f + offsetX;
  const Rectangle source = {0.0f, 0.0f, static_cast<float>(texture.width),
                            static_cast<float>(texture.height)};
  for (float y = destination.y; y < destination.y + destination.height; y += drawHeight) {
    const float remainingHeight = destination.y + destination.height - y;
    const float segmentHeight = std::min(drawHeight, remainingHeight);
    const float sourceHeight =
        texture.height * (segmentHeight / std::max(drawHeight, 1.0f));
    const Rectangle segmentSource = {source.x, source.y, source.width, sourceHeight};
    const Rectangle segmentDestination = {x, y, drawWidth, segmentHeight};
    DrawTexturePro(texture, segmentSource, segmentDestination, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
  }
}

inline void DrawTextureRepeatHorizontalCentered(Texture2D texture, Rectangle destination,
                                                float drawHeight, float offsetY = 0.0f) {
  if (!TextureLoaded(texture) || texture.height <= 0 || texture.width <= 0) {
    return;
  }

  const float drawWidth =
      drawHeight * static_cast<float>(texture.width) / static_cast<float>(texture.height);
  const float y = destination.y + (destination.height - drawHeight) * 0.5f + offsetY;
  for (float x = destination.x; x < destination.x + destination.width; x += drawWidth) {
    const float remainingWidth = destination.x + destination.width - x;
    const float segmentWidth = std::min(drawWidth, remainingWidth);
    const float sourceWidth =
        texture.width * (segmentWidth / std::max(drawWidth, 1.0f));
    const Rectangle segmentSource = {0.0f, 0.0f, sourceWidth,
                                     static_cast<float>(texture.height)};
    const Rectangle segmentDestination = {x, y, segmentWidth, drawHeight};
    DrawTexturePro(texture, segmentSource, segmentDestination, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
  }
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

inline void DrawCityVisuals(const GameAssets& assets, const CityMap& city,
                            const RoadArtTuning& roadArtTuning) {
  for (const CityVisual& visual : city.visuals) {
    const Texture2D texture = GetCityTexture(assets, visual.sprite);
    const Rectangle destination = {visual.x, visual.y, visual.width, visual.height};
    if (TextureLoaded(texture)) {
      if (IsRoadSprite(visual.sprite)) {
        const RoadPieceArt art = GetRoadPieceArt(roadArtTuning, visual.sprite);
        const float scale = static_cast<float>(art.scalePercent) / 100.0f;
        const float footprintScale = static_cast<float>(art.footprintPercent) / 100.0f;
        const Rectangle footprint =
            OffsetRectangle(ScaleRectangleAnchored(destination, footprintScale, art.anchorX,
                                                   art.anchorY),
                            static_cast<float>(art.offsetX), static_cast<float>(art.offsetY));
        if (art.drawMode == RoadArtDrawMode::kRepeatHorizontal) {
          DrawTextureRepeatHorizontalCentered(texture, footprint,
                                              static_cast<float>(texture.height) * scale,
                                              0.0f);
        } else if (art.drawMode == RoadArtDrawMode::kRepeatVertical) {
          DrawTextureRepeatVerticalCentered(texture, footprint,
                                            static_cast<float>(texture.width) * scale,
                                            0.0f);
        } else {
          DrawTextureCover(
              texture,
              ScaleRectangleAnchored(footprint, scale, art.anchorX, art.anchorY),
              visual.rotationDeg);
        }
      } else {
        DrawTextureCover(texture, destination, visual.rotationDeg);
      }
    } else {
      DrawRectangleLinesEx(destination, 3.0f, Color{255, 80, 80, 255});
    }
  }
}

inline void DrawCityCoins(const GameAssets& assets, const CityMap& city) {
  for (const CityCoin& coin : city.coins) {
    if (coin.collected &&
        coin.collectAnimationSeconds >= kGameCoinCollectAnimationSeconds) {
      continue;
    }
    const bool animating = coin.collected;
    const float progress =
        animating
            ? std::clamp(coin.collectAnimationSeconds / kGameCoinCollectAnimationSeconds, 0.0f,
                         1.0f)
            : 0.0f;
    const float jump = animating ? std::sin(progress * 3.14159265358979323846f) : 0.0f;
    const float idleBob = animating ? 0.0f : std::sin(GetTime() * 3.2 + coin.x * 0.03f) * 4.0f;
    const float coinSize = animating ? 37.0f + 37.0f * jump : 37.0f;
    const float yOffset = animating ? -72.0f * jump : idleBob;
    const unsigned char alpha =
        animating ? static_cast<unsigned char>(std::clamp((1.0f - progress) * 255.0f, 0.0f, 255.0f))
                  : 255;
    const Color tint = Color{255, 255, 255, alpha};
    if (TextureLoaded(assets.coin)) {
      const Rectangle source = {0.0f, 0.0f, static_cast<float>(assets.coin.width),
                                static_cast<float>(assets.coin.height)};
      const Rectangle destination = {coin.x, coin.y + yOffset, coinSize, coinSize};
      DrawTexturePro(assets.coin, source, destination, Vector2{coinSize * 0.5f, coinSize * 0.5f},
                     0.0f, tint);
    } else {
      DrawCircleV(Vector2{coin.x, coin.y + yOffset}, coinSize * 0.38f,
                  Color{255, 203, 64, alpha});
    }
  }
}

inline void DrawDustParticles(const GameAssets& assets, const GameState& game) {
  for (const DustParticle& particle : game.dustParticles) {
    if (!particle.active || particle.maxLifeSeconds <= 0.0f) {
      continue;
    }
    const float lifeT = std::clamp(particle.lifeSeconds / particle.maxLifeSeconds, 0.0f, 1.0f);
    const float alphaScale = 1.0f - lifeT;
    const float radius = particle.radius * (0.92f + lifeT * 0.55f);
    if (TextureLoaded(assets.dustCloud)) {
      const Rectangle source = {0.0f, 0.0f, static_cast<float>(assets.dustCloud.width),
                                static_cast<float>(assets.dustCloud.height)};
      const float drawSize = radius * 1.95f;
      const Rectangle destination = {particle.position.x, particle.position.y, drawSize, drawSize};
      const Color tint = {255, 255, 255,
                          static_cast<unsigned char>(std::clamp(alphaScale * 155.0f, 0.0f, 255.0f))};
      DrawTexturePro(assets.dustCloud, source, destination,
                     Vector2{drawSize * 0.5f, drawSize * 0.5f}, 0.0f, tint);
    } else {
      DrawCircleV(Vector2{particle.position.x, particle.position.y}, radius,
                  Color{204, 184, 148, static_cast<unsigned char>(alphaScale * 110.0f)});
      DrawCircleV(
          Vector2{particle.position.x - radius * 0.18f, particle.position.y - radius * 0.12f},
          radius * 0.62f,
          Color{228, 212, 186, static_cast<unsigned char>(alphaScale * 70.0f)});
    }
  }
}

inline void DrawDebugArc(Vector2 center, float radius, float startDeg, float endDeg,
                         Color color) {
  constexpr int segments = 32;
  Vector2 previous = PointOnCircle(center, radius, startDeg);
  for (int i = 1; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);
    const float angle = startDeg + (endDeg - startDeg) * t;
    const Vector2 current = PointOnCircle(center, radius, angle);
    DrawLineEx(previous, current, 3.0f, color);
    previous = current;
  }
}

inline void DrawDebugRadial(Vector2 center, float innerRadius, float outerRadius, float angleDeg,
                            Color color) {
  DrawLineEx(PointOnCircle(center, innerRadius, angleDeg),
             PointOnCircle(center, outerRadius, angleDeg), 3.0f, color);
}

inline void DrawDebugRoadCurve(float tileX, float tileY, Vector2 center, float startDeg,
                               float endDeg, Color color) {
  const float innerRadius = kCityCurveRoadRadius - kCityRoadHalfWidth;
  const float outerRadius = kCityCurveRoadRadius + kCityRoadHalfWidth;
  DrawDebugArc(center, innerRadius, startDeg, endDeg, color);
  DrawDebugArc(center, outerRadius, startDeg, endDeg, color);
  DrawDebugRadial(center, innerRadius, outerRadius, startDeg, color);
  DrawDebugRadial(center, innerRadius, outerRadius, endDeg, color);
  DrawRectangleLinesEx(Rectangle{tileX, tileY, kCityTileSize, kCityTileSize}, 1.0f,
                       Color{255, 255, 255, 70});
}

inline void DrawCityCollisionDebug(const CityMap& city, const GameState& game) {
  const Color roadColor = Color{0, 220, 255, 230};
  const Color roadFill = Color{0, 220, 255, 38};
  const Color obstacleColor = Color{255, 132, 54, 230};
  const Color carColor = Color{255, 244, 205, 230};
  const float center = kCityTileSize * 0.5f;

  for (const CityRoadTile& road : city.roads) {
    const float tileX = road.column * kCityTileSize;
    const float tileY = road.row * kCityTileSize;
    const Rectangle horizontal = {tileX, tileY + center - kCityRoadHalfWidth,
                                  kCityTileSize, kCityRoadHalfWidth * 2.0f};
    const Rectangle vertical = {tileX + center - kCityRoadHalfWidth, tileY,
                                kCityRoadHalfWidth * 2.0f, kCityTileSize};

    switch (road.kind) {
      case CityRoadKind::kHorizontal:
        DrawRectangleRec(horizontal, roadFill);
        DrawRectangleLinesEx(horizontal, 3.0f, roadColor);
        break;
      case CityRoadKind::kVertical:
        DrawRectangleRec(vertical, roadFill);
        DrawRectangleLinesEx(vertical, 3.0f, roadColor);
        break;
      case CityRoadKind::kIntersection:
        DrawRectangleRec(horizontal, roadFill);
        DrawRectangleRec(vertical, roadFill);
        DrawRectangleLinesEx(horizontal, 3.0f, roadColor);
        DrawRectangleLinesEx(vertical, 3.0f, roadColor);
        break;
      case CityRoadKind::kCurveBottomRight:
        DrawDebugRoadCurve(tileX, tileY, Vector2{tileX + kCityTileSize, tileY + kCityTileSize},
                           180.0f, 270.0f, roadColor);
        break;
      case CityRoadKind::kCurveBottomLeft:
        DrawDebugRoadCurve(tileX, tileY, Vector2{tileX, tileY + kCityTileSize}, 270.0f,
                           360.0f, roadColor);
        break;
      case CityRoadKind::kCurveTopRight:
        DrawDebugRoadCurve(tileX, tileY, Vector2{tileX + kCityTileSize, tileY}, 90.0f,
                           180.0f, roadColor);
        break;
      case CityRoadKind::kCurveTopLeft:
        DrawDebugRoadCurve(tileX, tileY, Vector2{tileX, tileY}, 0.0f, 90.0f, roadColor);
        break;
    }
  }

  for (const CityObstacle& obstacle : city.obstacles) {
    if (obstacle.circle) {
      DrawCircleLines(static_cast<int>(obstacle.x + obstacle.width * 0.5f),
                      static_cast<int>(obstacle.y + obstacle.height * 0.5f),
                      obstacle.width * 0.5f, obstacleColor);
    } else {
      DrawRectangleLinesEx(Rectangle{obstacle.x, obstacle.y, obstacle.width, obstacle.height},
                           3.0f, obstacleColor);
    }
  }

  DrawCircleLines(static_cast<int>(game.carPosition.x), static_cast<int>(game.carPosition.y),
                  kGameCarCollisionRadius, carColor);
}

inline void DrawRoadArtEditorOverlay(const RoadArtTuning& tuning,
                                     const RoadArtEditorState& editor,
                                     int screenWidth, int screenHeight) {
  if (!editor.active) {
    return;
  }

  const RoadPieceArt& art = GetRoadPieceArt(tuning, editor.targetSprite);
  const RoadArtEditorField field = GetRoadArtEditorField(editor);
  const int value = GetRoadArtEditorFieldValue(art, field);
  const int panelWidth = 560;
  const int panelHeight = 206;
  const int x = (screenWidth - panelWidth) / 2;
  const int y = screenHeight - panelHeight - 24;

  DrawRectangle(x, y, panelWidth, panelHeight, Color{12, 20, 24, 220});
  DrawRectangleLinesEx(Rectangle{static_cast<float>(x), static_cast<float>(y),
                                 static_cast<float>(panelWidth), static_cast<float>(panelHeight)},
                       2.0f, Color{246, 187, 87, 255});
  DrawText("ROAD ART EDITOR", x + 18, y + 16, 22, Color{255, 244, 205, 255});
  DrawText(TextFormat("target: %s", ToConfigToken(editor.targetSprite)), x + 18, y + 48, 18,
           editor.selectingAsset ? Color{246, 187, 87, 255} : Color{232, 236, 224, 255});
  DrawText(TextFormat("field: %s = %d", ToDisplayName(field), value), x + 18, y + 76, 18,
           editor.selectingAsset ? Color{232, 236, 224, 255} : Color{246, 187, 87, 255});
  DrawText(TextFormat("scale:%d%%  footprint:%d%%  offset:%d,%d  anchor:%d,%d",
                      art.scalePercent, art.footprintPercent, art.offsetX, art.offsetY,
                      art.anchorX, art.anchorY),
           x + 18, y + 104, 17, Color{195, 214, 204, 255});
  DrawText(editor.selectingAsset ? "selector: asset   up/down: previous/next road piece"
                                 : "selector: field   up/down: previous/next value field",
           x + 18, y + 136, 16, Color{176, 196, 186, 255});
  DrawText("`/~: toggle selector   left/right: -/+1   E: exit   auto-saves",
           x + 18, y + 162, 16, Color{176, 196, 186, 255});
}
}  // namespace game_view_detail

inline void DrawGame(const GameState& game, const GameAssets& assets, const CityMap& city,
                     const RoadArtTuning& roadArtTuning,
                     const RoadArtEditorState& roadArtEditor,
                     bool hasFreshPackets, bool hasAnyPacket, bool isBroadcastingForLoss,
                     const std::string& localIpText, int screenWidth, int screenHeight,
                     float cameraZoomScale) {
  ClearBackground(Color{60, 133, 126, 255});

  const bool hasCity = city.columns > 0 && city.rows > 0;
  const float mapWidth = hasCity ? city.columns * kCityTileSize : kGameMapSize;
  const float mapHeight = hasCity ? city.rows * kCityTileSize : kGameMapSize;
  const Camera2D camera = game_view_detail::BuildGameCamera(
      game, screenWidth, screenHeight, mapWidth, mapHeight, cameraZoomScale);
  BeginMode2D(camera);
  if (hasCity) {
    game_view_detail::DrawCityTerrain(assets, city);
    game_view_detail::DrawCityVisuals(assets, city, roadArtTuning);
    game_view_detail::DrawCityCoins(assets, city);
    if (roadArtEditor.active) {
      game_view_detail::DrawCityCollisionDebug(city, game);
    }
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

  game_view_detail::DrawDustParticles(assets, game);

  if (TextureLoaded(assets.car)) {
    const Rectangle source = {0.0f, 0.0f, static_cast<float>(assets.car.width),
                              static_cast<float>(assets.car.height)};
    const float carDrawWidth = 29.0f;
    const float carDrawHeight =
        carDrawWidth * static_cast<float>(assets.car.height) / static_cast<float>(assets.car.width);
    game_view_detail::DrawCarFrontWheels(assets, game, carDrawWidth, carDrawHeight);
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
    DrawText(TextFormat("coins: %d / %d", game.score, static_cast<int>(city.coins.size())), 40, 74,
             22, Color{232, 236, 224, 255});
  } else {
    DrawText(TextFormat("coins: %d / %d", game.score, static_cast<int>(game.coins.size())), 40, 74,
             22, Color{232, 236, 224, 255});
  }
  if (hasCity) {
    DrawText(TextFormat("drive: %s   %s   speed: %.0f",
                        game.driveMode == DriveMode::kAuto ? "auto" : "button",
                        game.onRoad ? "road" : "off road", game.carSpeed),
             40, 104, 20,
             game.onRoad ? Color{232, 236, 224, 255} : Color{246, 187, 87, 255});
  } else {
    DrawText(TextFormat("drive: %s   speed: %.0f",
                        game.driveMode == DriveMode::kAuto ? "auto" : "button", game.carSpeed),
             40, 104, 20, Color{232, 236, 224, 255});
  }
  if (hasCity && game.hitObstacle) {
    DrawText("bonk", 250, 104, 20, Color{255, 180, 120, 255});
  }

  DrawRectangle(rightPanelX, 20, rightPanelWidth, 196, Color{18, 28, 32, 185});
  DrawText(platform::kGameHelp, rightTextX, 36, 22, Color{232, 236, 224, 255});
  DrawText("A: auto/button drive   SPACE: center", rightTextX, 66, 19,
           Color{195, 214, 204, 255});
  DrawText("-/=: zoom   0: reset zoom", rightTextX, 94, 19, Color{195, 214, 204, 255});
  DrawText("E: editor   `/~: asset/field", rightTextX, 120, 19, Color{195, 214, 204, 255});
  DrawText(hasFreshPackets ? "UDP controller active" : "keyboard fallback", rightTextX, 146, 19,
           hasFreshPackets ? Color{104, 230, 141, 255} : Color{246, 187, 87, 255});
  DrawText(TextFormat("FPS: %d   zoom: %.2fx", GetFPS(), cameraZoomScale), rightTextX, 168, 19,
           Color{255, 244, 205, 255});
  DrawText(localIpText.c_str(), rightTextX, 192, 16, Color{176, 196, 186, 255});

  game_view_detail::DrawRoadArtEditorOverlay(roadArtTuning, roadArtEditor, screenWidth,
                                             screenHeight);

  if (hasAnyPacket && !hasFreshPackets) {
    DrawRectangle(0, 0, screenWidth, 38, Color{153, 48, 32, 220});
    DrawText(isBroadcastingForLoss ? "Controller connection lost - rediscovering..."
                                   : "Controller connection lost",
             20, 9, 20, Color{255, 240, 232, 255});
  }
}
