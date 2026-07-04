#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

constexpr float kCityAuthoringTileSize = 512.0f;
constexpr float kCityTileSize = 256.0f;
constexpr float kCityAuthoringToWorldScale = kCityTileSize / kCityAuthoringTileSize;

enum class CitySprite {
  kRoadHorizontal,
  kRoadVertical,
  kRoadIntersection,
  kRoadCurveBottomRight,
  kRoadCurveBottomLeft,
  kRoadCurveTopRight,
  kRoadCurveTopLeft,
  kCoinStar,
  kTreeRound,
  kTreeEvergreen,
  kBushCluster,
  kBuildingHouse,
  kBuildingShop,
  kBuildingSchool,
  kBuildingPolice,
  kBuildingFireStation,
  kBuildingLibrary,
  kKrakenPop,
  kKrakenRoad,
};

enum class CityKrakenKind {
  kPop,
  kRoad,
};

enum class CityRoadKind {
  kHorizontal,
  kVertical,
  kIntersection,
  kCurveBottomRight,
  kCurveBottomLeft,
  kCurveTopRight,
  kCurveTopLeft,
};

struct CityVisual {
  CitySprite sprite = CitySprite::kRoadHorizontal;
  float x = 0.0f;
  float y = 0.0f;
  float width = kCityTileSize;
  float height = kCityTileSize;
  float rotationDeg = 0.0f;
};

struct CityRoadTile {
  CityRoadKind kind = CityRoadKind::kHorizontal;
  int column = 0;
  int row = 0;
};

struct CityCoin {
  float x = 0.0f;
  float y = 0.0f;
  bool collected = false;
  float collectAnimationSeconds = 0.0f;
};

struct CityKraken {
  CityKrakenKind kind = CityKrakenKind::kPop;
  float x = 0.0f;
  float y = 0.0f;
  float size = kCityTileSize;
  bool collected = false;
  float collectAnimationSeconds = 0.0f;
};

struct CityObstacle {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  bool circle = false;
};

struct CityMap {
  int columns = 0;
  int rows = 0;
  bool hasPlayerSpawn = false;
  float playerSpawnX = kCityTileSize * 0.5f;
  float playerSpawnY = kCityTileSize * 0.5f;
  std::vector<CityVisual> visuals;
  std::vector<CityRoadTile> roads;
  std::vector<CityCoin> coins;
  std::vector<CityKraken> krakens;
  std::vector<CityObstacle> obstacles;
};

namespace city_map_detail {
inline void AddWarning(std::vector<std::string>* warnings, const std::string& warning) {
  if (warnings != nullptr) {
    warnings->push_back(warning);
  }
}

inline std::string Trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  return value;
}

inline std::vector<std::string> Split(const std::string& value, char delimiter) {
  std::vector<std::string> parts;
  std::stringstream stream(value);
  std::string part;
  while (std::getline(stream, part, delimiter)) {
    parts.push_back(Trim(part));
  }
  return parts;
}

inline float ParseFloatOrDefault(const std::string& value, float fallback) {
  if (value.empty()) {
    return fallback;
  }

  char* end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  return end == value.c_str() ? fallback : parsed;
}

inline float AuthoringToWorld(float value) {
  return value * kCityAuthoringToWorldScale;
}

inline void ParseOffsetAndScale(const std::string& token, std::string* name, float* offsetX,
                                float* offsetY, float* scale) {
  *name = token;
  *offsetX = kCityAuthoringTileSize * 0.5f;
  *offsetY = kCityAuthoringTileSize * 0.5f;
  *scale = 1.0f;

  const size_t at = name->find('@');
  if (at == std::string::npos) {
    return;
  }

  std::string suffix = name->substr(at + 1);
  *name = name->substr(0, at);

  const size_t star = suffix.find('*');
  if (star != std::string::npos) {
    *scale = ParseFloatOrDefault(suffix.substr(star + 1), *scale);
    suffix = suffix.substr(0, star);
  }

  const size_t colon = suffix.find(':');
  if (colon != std::string::npos) {
    *offsetX = ParseFloatOrDefault(suffix.substr(0, colon), *offsetX);
    *offsetY = ParseFloatOrDefault(suffix.substr(colon + 1), *offsetY);
  }
}

inline bool TryAddRoad(const std::string& token, int column, int row, CityMap* city) {
  CitySprite sprite = CitySprite::kRoadHorizontal;
  CityRoadKind kind = CityRoadKind::kHorizontal;
  if (token == "r_h") {
    sprite = CitySprite::kRoadHorizontal;
    kind = CityRoadKind::kHorizontal;
  } else if (token == "r_v") {
    sprite = CitySprite::kRoadVertical;
    kind = CityRoadKind::kVertical;
  } else if (token == "r_x") {
    sprite = CitySprite::kRoadIntersection;
    kind = CityRoadKind::kIntersection;
  } else if (token == "r_br") {
    sprite = CitySprite::kRoadCurveBottomRight;
    kind = CityRoadKind::kCurveBottomRight;
  } else if (token == "r_bl") {
    sprite = CitySprite::kRoadCurveBottomLeft;
    kind = CityRoadKind::kCurveBottomLeft;
  } else if (token == "r_tr") {
    sprite = CitySprite::kRoadCurveTopRight;
    kind = CityRoadKind::kCurveTopRight;
  } else if (token == "r_tl") {
    sprite = CitySprite::kRoadCurveTopLeft;
    kind = CityRoadKind::kCurveTopLeft;
  } else {
    return false;
  }

  city->visuals.push_back(CityVisual{sprite, column * kCityTileSize, row * kCityTileSize,
                                     kCityTileSize, kCityTileSize, 0.0f});
  city->roads.push_back(CityRoadTile{kind, column, row});
  return true;
}

inline void AddCenteredVisual(CityMap* city, CitySprite sprite, float centerX, float centerY,
                              float size) {
  city->visuals.push_back(
      CityVisual{sprite, centerX - size * 0.5f, centerY - size * 0.5f, size, size, 0.0f});
}

inline void AddRectObstacle(CityMap* city, float centerX, float centerY, float size,
                            float widthFraction, float heightFraction,
                            float offsetYFraction = 0.0f) {
  const float width = size * widthFraction;
  const float height = size * heightFraction;
  const float offsetY = size * offsetYFraction;
  city->obstacles.push_back(
      CityObstacle{centerX - width * 0.5f, centerY - height * 0.5f + offsetY, width, height, false});
}

inline void AddCircleObstacle(CityMap* city, float centerX, float centerY, float diameter) {
  city->obstacles.push_back(
      CityObstacle{centerX - diameter * 0.5f, centerY - diameter * 0.5f, diameter, diameter, true});
}

inline bool TryAddObject(const std::string& token, int column, int row, CityMap* city) {
  std::string name;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float scale = 1.0f;
  ParseOffsetAndScale(token, &name, &offsetX, &offsetY, &scale);

  CitySprite sprite = CitySprite::kCoinStar;
  float size = 120.0f;
  const float centerX = column * kCityTileSize + city_map_detail::AuthoringToWorld(offsetX);
  const float centerY = row * kCityTileSize + city_map_detail::AuthoringToWorld(offsetY);
  if (name == "coin:star") {
    city->coins.push_back(CityCoin{centerX, centerY, false, 0.0f});
    return true;
  } else if (name == "kraken:pop") {
    city->krakens.push_back(CityKraken{CityKrakenKind::kPop, centerX, centerY,
                                       340.0f * scale * kCityAuthoringToWorldScale, false, 0.0f});
    return true;
  } else if (name == "kraken:road") {
    city->krakens.push_back(CityKraken{CityKrakenKind::kRoad, centerX, centerY,
                                       380.0f * scale * kCityAuthoringToWorldScale, false, 0.0f});
    return true;
  } else if (name == "tree:round") {
    sprite = CitySprite::kTreeRound;
    size = 230.0f;
  } else if (name == "tree:evergreen") {
    sprite = CitySprite::kTreeEvergreen;
    size = 250.0f;
  } else if (name == "bush") {
    sprite = CitySprite::kBushCluster;
    size = 210.0f;
  } else if (name == "house") {
    sprite = CitySprite::kBuildingHouse;
    size = 360.0f;
  } else if (name == "shop") {
    sprite = CitySprite::kBuildingShop;
    size = 360.0f;
  } else if (name == "school") {
    sprite = CitySprite::kBuildingSchool;
    size = 390.0f;
  } else if (name == "police") {
    sprite = CitySprite::kBuildingPolice;
    size = 390.0f;
  } else if (name == "fire_station") {
    sprite = CitySprite::kBuildingFireStation;
    size = 390.0f;
  } else if (name == "library") {
    sprite = CitySprite::kBuildingLibrary;
    size = 390.0f;
  } else if (name == "spawn:player") {
    city->hasPlayerSpawn = true;
    city->playerSpawnX = centerX;
    city->playerSpawnY = centerY;
    return true;
  } else if (name == "grass") {
    return true;
  } else {
    return false;
  }

  size *= scale * kCityAuthoringToWorldScale;
  AddCenteredVisual(city, sprite, centerX, centerY, size);
  if (name == "tree:round" || name == "tree:evergreen" || name == "bush") {
    AddCircleObstacle(city, centerX, centerY, size * 0.58f);
  } else {
    AddRectObstacle(city, centerX, centerY, size, 0.56f, 0.46f, 0.10f);
  }
  return true;
}
}  // namespace city_map_detail

inline CityMap LoadCityMap(const std::string& path, std::vector<std::string>* warnings = nullptr) {
  CityMap city;
  std::ifstream file(path);
  if (!file.is_open()) {
    city_map_detail::AddWarning(warnings, "Could not open city map: " + path);
    return city;
  }

  std::string line;
  int row = 0;
  while (std::getline(file, line)) {
    const std::vector<std::string> cells = city_map_detail::Split(line, ',');
    city.columns = std::max(city.columns, static_cast<int>(cells.size()));
    for (int column = 0; column < static_cast<int>(cells.size()); ++column) {
      for (const std::string& token : city_map_detail::Split(cells[column], '|')) {
        if (token.empty() || token == "grass") {
          continue;
        }
        if (!city_map_detail::TryAddRoad(token, column, row, &city)) {
          if (!city_map_detail::TryAddObject(token, column, row, &city)) {
            std::ostringstream warning;
            warning << "Unknown city token '" << token << "' at row " << (row + 1)
                    << ", column " << (column + 1);
            city_map_detail::AddWarning(warnings, warning.str());
          }
        }
      }
    }
    ++row;
  }

  city.rows = row;
  return city;
}
