#pragma once

#include <string>
#include <vector>

#include "city_map.h"
#include "raylib.h"

struct GameAssets {
  Texture2D terrain = {};
  Texture2D roadHorizontal = {};
  Texture2D roadVertical = {};
  Texture2D roadIntersection = {};
  Texture2D roadCurveBottomRight = {};
  Texture2D roadCurveBottomLeft = {};
  Texture2D roadCurveTopRight = {};
  Texture2D roadCurveTopLeft = {};
  Texture2D car = {};
  Texture2D carTire = {};
  Texture2D coin = {};
  Texture2D dustCloud = {};
  Texture2D treeRound = {};
  Texture2D treeEvergreen = {};
  Texture2D bushCluster = {};
  Texture2D buildingHouse = {};
  Texture2D buildingShop = {};
  Texture2D buildingSchool = {};
  Texture2D buildingPolice = {};
  Texture2D buildingFireStation = {};
  Texture2D buildingLibrary = {};
  Texture2D krakenPop = {};
  Texture2D krakenRoad = {};
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

inline std::string FindCityPath(const char* filename) {
  return FindAssetPath("assets/cities", filename);
}

inline std::string FindConfigPath(const char* filename) {
  return FindAssetPath("assets/config", filename);
}

inline Texture2D LoadSpriteTexture(const char* filename, int filter = TEXTURE_FILTER_BILINEAR) {
  Texture2D texture = LoadTexture(FindSpritePath(filename).c_str());
  if (TextureLoaded(texture)) {
    SetTextureFilter(texture, filter);
  }
  return texture;
}
}  // namespace game_assets_detail

inline GameAssets LoadGameAssets() {
  GameAssets assets;
  assets.terrain = game_assets_detail::LoadSpriteTexture("terrain_carpet_tilemirror.png");
  assets.roadHorizontal = game_assets_detail::LoadSpriteTexture("road_horizontal_repeat.png");
  assets.roadVertical = game_assets_detail::LoadSpriteTexture("road_vertical_repeat.png");
  assets.roadIntersection =
      game_assets_detail::LoadSpriteTexture("road_intersection_4way_crosswalks.png");
  assets.roadCurveBottomRight =
      game_assets_detail::LoadSpriteTexture("road_curve_bottom_right.png");
  assets.roadCurveBottomLeft = game_assets_detail::LoadSpriteTexture("road_curve_bottom_left.png");
  assets.roadCurveTopRight = game_assets_detail::LoadSpriteTexture("road_curve_top_right.png");
  assets.roadCurveTopLeft = game_assets_detail::LoadSpriteTexture("road_curve_top_left.png");
  assets.car = game_assets_detail::LoadSpriteTexture("toy_sports_car.png");
  assets.carTire = game_assets_detail::LoadSpriteTexture("toy_car_tire.png");
  assets.coin = game_assets_detail::LoadSpriteTexture("coin_star.png");
  assets.dustCloud = game_assets_detail::LoadSpriteTexture("dust_cloud_top_round.png");
  assets.treeRound = game_assets_detail::LoadSpriteTexture("prop_tree_round_ai_01.png");
  assets.treeEvergreen = game_assets_detail::LoadSpriteTexture("prop_evergreen.png");
  assets.bushCluster = game_assets_detail::LoadSpriteTexture("prop_bush_cluster.png");
  assets.buildingHouse = game_assets_detail::LoadSpriteTexture("building_house.png");
  assets.buildingShop = game_assets_detail::LoadSpriteTexture("building_shop.png");
  assets.buildingSchool = game_assets_detail::LoadSpriteTexture("building_school_ai_01.png");
  assets.buildingPolice = game_assets_detail::LoadSpriteTexture("building_police_station_ai_01.png");
  assets.buildingFireStation = game_assets_detail::LoadSpriteTexture("building_fire_station.png");
  assets.buildingLibrary = game_assets_detail::LoadSpriteTexture("building_library.png");
  assets.krakenPop = game_assets_detail::LoadSpriteTexture("kraken_octopus_pop_sheet.png");
  assets.krakenRoad = game_assets_detail::LoadSpriteTexture("kraken_octopus_road_sheet.png");
  return assets;
}

inline Texture2D GetCityTexture(const GameAssets& assets, CitySprite sprite) {
  switch (sprite) {
    case CitySprite::kRoadHorizontal:
      return assets.roadHorizontal;
    case CitySprite::kRoadVertical:
      return assets.roadVertical;
    case CitySprite::kRoadIntersection:
      return assets.roadIntersection;
    case CitySprite::kRoadCurveBottomRight:
      return assets.roadCurveBottomRight;
    case CitySprite::kRoadCurveBottomLeft:
      return assets.roadCurveBottomLeft;
    case CitySprite::kRoadCurveTopRight:
      return assets.roadCurveTopRight;
    case CitySprite::kRoadCurveTopLeft:
      return assets.roadCurveTopLeft;
    case CitySprite::kCoinStar:
      return assets.coin;
    case CitySprite::kTreeRound:
      return assets.treeRound;
    case CitySprite::kTreeEvergreen:
      return assets.treeEvergreen;
    case CitySprite::kBushCluster:
      return assets.bushCluster;
    case CitySprite::kBuildingHouse:
      return assets.buildingHouse;
    case CitySprite::kBuildingShop:
      return assets.buildingShop;
    case CitySprite::kBuildingSchool:
      return assets.buildingSchool;
    case CitySprite::kBuildingPolice:
      return assets.buildingPolice;
    case CitySprite::kBuildingFireStation:
      return assets.buildingFireStation;
    case CitySprite::kBuildingLibrary:
      return assets.buildingLibrary;
    case CitySprite::kKrakenPop:
      return assets.krakenPop;
    case CitySprite::kKrakenRoad:
      return assets.krakenRoad;
  }

  return {};
}

inline void UnloadIfLoaded(Texture2D* texture) {
  if (TextureLoaded(*texture)) {
    UnloadTexture(*texture);
    *texture = {};
  }
}

inline void UnloadGameAssets(GameAssets* assets) {
  UnloadIfLoaded(&assets->terrain);
  UnloadIfLoaded(&assets->roadHorizontal);
  UnloadIfLoaded(&assets->roadVertical);
  UnloadIfLoaded(&assets->roadIntersection);
  UnloadIfLoaded(&assets->roadCurveBottomRight);
  UnloadIfLoaded(&assets->roadCurveBottomLeft);
  UnloadIfLoaded(&assets->roadCurveTopRight);
  UnloadIfLoaded(&assets->roadCurveTopLeft);
  UnloadIfLoaded(&assets->car);
  UnloadIfLoaded(&assets->carTire);
  UnloadIfLoaded(&assets->coin);
  UnloadIfLoaded(&assets->dustCloud);
  UnloadIfLoaded(&assets->treeRound);
  UnloadIfLoaded(&assets->treeEvergreen);
  UnloadIfLoaded(&assets->bushCluster);
  UnloadIfLoaded(&assets->buildingHouse);
  UnloadIfLoaded(&assets->buildingShop);
  UnloadIfLoaded(&assets->buildingSchool);
  UnloadIfLoaded(&assets->buildingPolice);
  UnloadIfLoaded(&assets->buildingFireStation);
  UnloadIfLoaded(&assets->buildingLibrary);
  UnloadIfLoaded(&assets->krakenPop);
  UnloadIfLoaded(&assets->krakenRoad);
}
