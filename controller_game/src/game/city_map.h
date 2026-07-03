#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

constexpr float kCityTileSize = 512.0f;

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
};

struct CityVisual {
  CitySprite sprite = CitySprite::kRoadHorizontal;
  float x = 0.0f;
  float y = 0.0f;
  float width = kCityTileSize;
  float height = kCityTileSize;
  float rotationDeg = 0.0f;
};

struct CityMap {
  int columns = 0;
  int rows = 0;
  std::vector<CityVisual> visuals;
};

namespace city_map_detail {
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

inline void ParseOffsetAndScale(const std::string& token, std::string* name, float* offsetX,
                                float* offsetY, float* scale) {
  *name = token;
  *offsetX = kCityTileSize * 0.5f;
  *offsetY = kCityTileSize * 0.5f;
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

inline bool TryAddRoadVisual(const std::string& token, int column, int row,
                             std::vector<CityVisual>* visuals) {
  CitySprite sprite = CitySprite::kRoadHorizontal;
  if (token == "r_h") {
    sprite = CitySprite::kRoadHorizontal;
  } else if (token == "r_v") {
    sprite = CitySprite::kRoadVertical;
  } else if (token == "r_x") {
    sprite = CitySprite::kRoadIntersection;
  } else if (token == "r_br") {
    sprite = CitySprite::kRoadCurveBottomRight;
  } else if (token == "r_bl") {
    sprite = CitySprite::kRoadCurveBottomLeft;
  } else if (token == "r_tr") {
    sprite = CitySprite::kRoadCurveTopRight;
  } else if (token == "r_tl") {
    sprite = CitySprite::kRoadCurveTopLeft;
  } else {
    return false;
  }

  visuals->push_back(CityVisual{sprite, column * kCityTileSize, row * kCityTileSize,
                                kCityTileSize, kCityTileSize, 0.0f});
  return true;
}

inline bool TryAddObjectVisual(const std::string& token, int column, int row,
                               std::vector<CityVisual>* visuals) {
  std::string name;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float scale = 1.0f;
  ParseOffsetAndScale(token, &name, &offsetX, &offsetY, &scale);

  CitySprite sprite = CitySprite::kCoinStar;
  float size = 120.0f;
  if (name == "coin:star") {
    sprite = CitySprite::kCoinStar;
    size = 74.0f;
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
  } else if (name.rfind("spawn:", 0) == 0 || name == "grass") {
    return true;
  } else {
    return false;
  }

  size *= scale;
  visuals->push_back(CityVisual{sprite, column * kCityTileSize + offsetX - size * 0.5f,
                                row * kCityTileSize + offsetY - size * 0.5f, size, size, 0.0f});
  return true;
}
}  // namespace city_map_detail

inline CityMap LoadCityMap(const std::string& path) {
  CityMap city;
  std::ifstream file(path);
  if (!file.is_open()) {
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
        if (!city_map_detail::TryAddRoadVisual(token, column, row, &city.visuals)) {
          city_map_detail::TryAddObjectVisual(token, column, row, &city.visuals);
        }
      }
    }
    ++row;
  }

  city.rows = row;
  return city;
}
