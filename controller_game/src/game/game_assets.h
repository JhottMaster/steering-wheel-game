#pragma once

#include <string>
#include <vector>

#include "raylib.h"

struct GameAssets {
  Texture2D map = {};
  Texture2D car = {};
  Texture2D coin = {};
};

inline bool TextureLoaded(Texture2D texture) {
  return texture.id != 0;
}

namespace game_assets_detail {
inline std::string FindAssetPath(const char* folder, const char* filename) {
  const std::vector<std::string> candidates = {
      std::string(folder) + "/" + filename,
      std::string("controller_game/") + folder + "/" + filename,
      std::string("../") + folder + "/" + filename,
      std::string("../../") + folder + "/" + filename,
  };

  for (const std::string& candidate : candidates) {
    if (FileExists(candidate.c_str())) {
      return candidate;
    }
  }

  return candidates.front();
}

inline std::string FindSpritePath(const char* filename) {
  return FindAssetPath("assets/sprites", filename);
}
}  // namespace game_assets_detail

inline GameAssets LoadGameAssets() {
  GameAssets assets;
  assets.map = LoadTexture(game_assets_detail::FindSpritePath("road_carpet_map_2.png").c_str());
  assets.car = LoadTexture(game_assets_detail::FindSpritePath("sports_car_top.png").c_str());
  assets.coin = LoadTexture(game_assets_detail::FindSpritePath("coin.png").c_str());
  return assets;
}

inline void UnloadGameAssets(GameAssets* assets) {
  if (TextureLoaded(assets->map)) {
    UnloadTexture(assets->map);
  }
  if (TextureLoaded(assets->car)) {
    UnloadTexture(assets->car);
  }
  if (TextureLoaded(assets->coin)) {
    UnloadTexture(assets->coin);
  }
}
