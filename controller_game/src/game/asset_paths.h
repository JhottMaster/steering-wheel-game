#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "raylib.h"

namespace game_asset_paths {
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

inline std::string FindSoundPath(const char* filename) {
  return FindAssetPath("assets/sounds", filename);
}

inline std::string FindCityPath(const char* filename) {
  return FindAssetPath("assets/cities", filename);
}

inline std::string FindConfigPath(const char* filename) {
  return FindAssetPath("assets/config", filename);
}

inline void LogMissingAsset(const char* kind, const char* filename, const std::string& path) {
  std::printf("[assets] missing %s '%s' (resolved path: %s)\n", kind, filename, path.c_str());
}
}  // namespace game_asset_paths
