#pragma once

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "city_map.h"

enum class RoadArtDrawMode {
  kStretch,
  kRepeatHorizontal,
  kRepeatVertical,
};

struct RoadPieceArt {
  int scalePercent = 100;
  int footprintPercent = 100;
  int offsetX = 0;
  int offsetY = 0;
  int anchorX = 0;
  int anchorY = 0;
  RoadArtDrawMode drawMode = RoadArtDrawMode::kStretch;
};

struct RoadArtTuning {
  RoadPieceArt horizontal =
      RoadPieceArt{41, 100, 0, -10, 0, 0, RoadArtDrawMode::kRepeatHorizontal};
  RoadPieceArt vertical =
      RoadPieceArt{50, 100, 0, 0, 0, 0, RoadArtDrawMode::kRepeatVertical};
  RoadPieceArt intersection = RoadPieceArt{100, 140, 0, 0, 0, 0, RoadArtDrawMode::kStretch};
  RoadPieceArt curveBottomRight = RoadPieceArt{100, 100, 0, 0, 1, 1, RoadArtDrawMode::kStretch};
  RoadPieceArt curveBottomLeft = RoadPieceArt{100, 100, 0, 0, -1, 1, RoadArtDrawMode::kStretch};
  RoadPieceArt curveTopRight = RoadPieceArt{100, 100, 0, 0, 1, -1, RoadArtDrawMode::kStretch};
  RoadPieceArt curveTopLeft = RoadPieceArt{100, 100, 0, 0, -1, -1, RoadArtDrawMode::kStretch};
};

enum class RoadArtEditorField {
  kScalePercent,
  kFootprintPercent,
  kOffsetX,
  kOffsetY,
  kAnchorX,
  kAnchorY,
};

constexpr int kRoadArtAnchorMin = -4;
constexpr int kRoadArtAnchorMax = 4;

struct RoadArtEditorState {
  bool active = false;
  CitySprite targetSprite = CitySprite::kRoadCurveBottomLeft;
  bool selectingAsset = false;
  int fieldIndex = 0;
};

namespace road_art_tuning_detail {
inline void AddWarning(std::vector<std::string>* warnings, const std::string& warning) {
  if (warnings != nullptr) {
    warnings->push_back(warning);
  }
}

inline std::vector<std::string> Split(const std::string& text, char delimiter) {
  std::vector<std::string> parts;
  std::stringstream stream(text);
  std::string part;
  while (std::getline(stream, part, delimiter)) {
    parts.push_back(part);
  }
  return parts;
}

inline std::string Trim(const std::string& text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  const size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(start, end - start + 1);
}

inline bool TryParseInt(const std::string& text, int* value) {
  const std::string trimmed = Trim(text);
  try {
    size_t parsedCharacters = 0;
    const int parsed = std::stoi(trimmed, &parsedCharacters);
    if (parsedCharacters == trimmed.size()) {
      *value = parsed;
      return true;
    }
  } catch (...) {
  }
  return false;
}

inline RoadArtDrawMode ParseDrawMode(const std::string& text) {
  const std::string token = Trim(text);
  if (token == "repeat_horizontal") {
    return RoadArtDrawMode::kRepeatHorizontal;
  }
  if (token == "repeat_vertical") {
    return RoadArtDrawMode::kRepeatVertical;
  }
  return RoadArtDrawMode::kStretch;
}
}  // namespace road_art_tuning_detail

inline const char* ToConfigToken(RoadArtDrawMode mode) {
  switch (mode) {
    case RoadArtDrawMode::kRepeatHorizontal:
      return "repeat_horizontal";
    case RoadArtDrawMode::kRepeatVertical:
      return "repeat_vertical";
    case RoadArtDrawMode::kStretch:
    default:
      return "stretch";
  }
}

inline const char* ToConfigToken(CitySprite sprite) {
  switch (sprite) {
    case CitySprite::kRoadHorizontal:
      return "road_horizontal";
    case CitySprite::kRoadVertical:
      return "road_vertical";
    case CitySprite::kRoadIntersection:
      return "road_intersection";
    case CitySprite::kRoadCurveBottomRight:
      return "road_curve_bottom_right";
    case CitySprite::kRoadCurveBottomLeft:
      return "road_curve_bottom_left";
    case CitySprite::kRoadCurveTopRight:
      return "road_curve_top_right";
    case CitySprite::kRoadCurveTopLeft:
      return "road_curve_top_left";
    default:
      return "unknown";
  }
}

inline RoadPieceArt* GetMutableRoadPieceArt(RoadArtTuning* tuning, CitySprite sprite) {
  switch (sprite) {
    case CitySprite::kRoadHorizontal:
      return &tuning->horizontal;
    case CitySprite::kRoadVertical:
      return &tuning->vertical;
    case CitySprite::kRoadIntersection:
      return &tuning->intersection;
    case CitySprite::kRoadCurveBottomRight:
      return &tuning->curveBottomRight;
    case CitySprite::kRoadCurveBottomLeft:
      return &tuning->curveBottomLeft;
    case CitySprite::kRoadCurveTopRight:
      return &tuning->curveTopRight;
    case CitySprite::kRoadCurveTopLeft:
      return &tuning->curveTopLeft;
    default:
      return &tuning->horizontal;
  }
}

inline const RoadPieceArt& GetRoadPieceArt(const RoadArtTuning& tuning, CitySprite sprite) {
  switch (sprite) {
    case CitySprite::kRoadHorizontal:
      return tuning.horizontal;
    case CitySprite::kRoadVertical:
      return tuning.vertical;
    case CitySprite::kRoadIntersection:
      return tuning.intersection;
    case CitySprite::kRoadCurveBottomRight:
      return tuning.curveBottomRight;
    case CitySprite::kRoadCurveBottomLeft:
      return tuning.curveBottomLeft;
    case CitySprite::kRoadCurveTopRight:
      return tuning.curveTopRight;
    case CitySprite::kRoadCurveTopLeft:
      return tuning.curveTopLeft;
    default:
      return tuning.horizontal;
  }
}

inline bool TryGetRoadSpriteFromConfigToken(const std::string& token, CitySprite* sprite) {
  const std::string name = road_art_tuning_detail::Trim(token);
  if (name == "road_horizontal") {
    *sprite = CitySprite::kRoadHorizontal;
  } else if (name == "road_vertical") {
    *sprite = CitySprite::kRoadVertical;
  } else if (name == "road_intersection") {
    *sprite = CitySprite::kRoadIntersection;
  } else if (name == "road_curve_bottom_right") {
    *sprite = CitySprite::kRoadCurveBottomRight;
  } else if (name == "road_curve_bottom_left") {
    *sprite = CitySprite::kRoadCurveBottomLeft;
  } else if (name == "road_curve_top_right") {
    *sprite = CitySprite::kRoadCurveTopRight;
  } else if (name == "road_curve_top_left") {
    *sprite = CitySprite::kRoadCurveTopLeft;
  } else {
    return false;
  }
  return true;
}

inline RoadArtTuning LoadRoadArtTuning(const std::string& path,
                                       std::vector<std::string>* warnings = nullptr) {
  RoadArtTuning tuning;
  std::ifstream file(path);
  if (!file.is_open()) {
    road_art_tuning_detail::AddWarning(warnings, "Could not open road art tuning: " + path);
    return tuning;
  }

  std::string line;
  bool firstLine = true;
  int lineNumber = 0;
  while (std::getline(file, line)) {
    ++lineNumber;
    if (line.empty()) {
      continue;
    }
    if (firstLine) {
      firstLine = false;
      if (line.find("sprite") != std::string::npos) {
        continue;
      }
    }

    const std::vector<std::string> cells = road_art_tuning_detail::Split(line, ',');
    if (cells.size() < 7) {
      std::ostringstream warning;
      warning << "Road art tuning row " << lineNumber << " has " << cells.size()
              << " columns; expected at least 7";
      road_art_tuning_detail::AddWarning(warnings, warning.str());
      continue;
    }

    CitySprite sprite = CitySprite::kRoadHorizontal;
    if (!TryGetRoadSpriteFromConfigToken(cells[0], &sprite)) {
      std::ostringstream warning;
      warning << "Unknown road art sprite '" << road_art_tuning_detail::Trim(cells[0])
              << "' at row " << lineNumber;
      road_art_tuning_detail::AddWarning(warnings, warning.str());
      continue;
    }

    RoadPieceArt art = *GetMutableRoadPieceArt(&tuning, sprite);
    if (!road_art_tuning_detail::TryParseInt(cells[1], &art.scalePercent)) {
      std::ostringstream warning;
      warning << "Invalid scale_percent for " << ToConfigToken(sprite) << " at row "
              << lineNumber;
      road_art_tuning_detail::AddWarning(warnings, warning.str());
    }
    int offsetColumn = 2;
    if (cells.size() >= 8) {
      if (!road_art_tuning_detail::TryParseInt(cells[2], &art.footprintPercent)) {
        std::ostringstream warning;
        warning << "Invalid footprint_percent for " << ToConfigToken(sprite) << " at row "
                << lineNumber;
        road_art_tuning_detail::AddWarning(warnings, warning.str());
      }
      offsetColumn = 3;
    }
    if (!road_art_tuning_detail::TryParseInt(cells[offsetColumn], &art.offsetX)) {
      std::ostringstream warning;
      warning << "Invalid offset_x for " << ToConfigToken(sprite) << " at row " << lineNumber;
      road_art_tuning_detail::AddWarning(warnings, warning.str());
    }
    if (!road_art_tuning_detail::TryParseInt(cells[offsetColumn + 1], &art.offsetY)) {
      std::ostringstream warning;
      warning << "Invalid offset_y for " << ToConfigToken(sprite) << " at row " << lineNumber;
      road_art_tuning_detail::AddWarning(warnings, warning.str());
    }
    if (!road_art_tuning_detail::TryParseInt(cells[offsetColumn + 2], &art.anchorX)) {
      std::ostringstream warning;
      warning << "Invalid anchor_x for " << ToConfigToken(sprite) << " at row " << lineNumber;
      road_art_tuning_detail::AddWarning(warnings, warning.str());
    }
    if (!road_art_tuning_detail::TryParseInt(cells[offsetColumn + 3], &art.anchorY)) {
      std::ostringstream warning;
      warning << "Invalid anchor_y for " << ToConfigToken(sprite) << " at row " << lineNumber;
      road_art_tuning_detail::AddWarning(warnings, warning.str());
    }
    art.drawMode = road_art_tuning_detail::ParseDrawMode(cells[offsetColumn + 4]);
    art.scalePercent = std::clamp(art.scalePercent, 1, 300);
    art.footprintPercent = std::clamp(art.footprintPercent, 1, 300);
    art.anchorX = std::clamp(art.anchorX, kRoadArtAnchorMin, kRoadArtAnchorMax);
    art.anchorY = std::clamp(art.anchorY, kRoadArtAnchorMin, kRoadArtAnchorMax);
    *GetMutableRoadPieceArt(&tuning, sprite) = art;
  }

  return tuning;
}

inline bool SaveRoadArtTuning(const std::string& path, const RoadArtTuning& tuning) {
  std::ofstream file(path, std::ios::trunc);
  if (!file.is_open()) {
    return false;
  }

  file << "sprite,scale_percent,footprint_percent,offset_x,offset_y,anchor_x,anchor_y,draw_mode\n";
  const CitySprite sprites[] = {
      CitySprite::kRoadHorizontal,       CitySprite::kRoadVertical,
      CitySprite::kRoadIntersection,     CitySprite::kRoadCurveBottomRight,
      CitySprite::kRoadCurveBottomLeft,  CitySprite::kRoadCurveTopRight,
      CitySprite::kRoadCurveTopLeft,
  };
  for (const CitySprite sprite : sprites) {
    const RoadPieceArt& art = GetRoadPieceArt(tuning, sprite);
    file << ToConfigToken(sprite) << "," << art.scalePercent << "," << art.footprintPercent
         << "," << art.offsetX << "," << art.offsetY << "," << art.anchorX << ","
         << art.anchorY << "," << ToConfigToken(art.drawMode) << "\n";
  }
  return true;
}

inline const CitySprite* GetRoadArtEditableSprites(int* count) {
  static const CitySprite sprites[] = {
      CitySprite::kRoadHorizontal,       CitySprite::kRoadVertical,
      CitySprite::kRoadIntersection,     CitySprite::kRoadCurveBottomRight,
      CitySprite::kRoadCurveBottomLeft,  CitySprite::kRoadCurveTopRight,
      CitySprite::kRoadCurveTopLeft,
  };
  *count = static_cast<int>(sizeof(sprites) / sizeof(sprites[0]));
  return sprites;
}

inline int GetRoadArtEditorSpriteIndex(CitySprite sprite) {
  int count = 0;
  const CitySprite* sprites = GetRoadArtEditableSprites(&count);
  for (int i = 0; i < count; ++i) {
    if (sprites[i] == sprite) {
      return i;
    }
  }
  return 0;
}

inline void AdjustRoadArtEditorSprite(RoadArtEditorState* editor, int delta) {
  int count = 0;
  const CitySprite* sprites = GetRoadArtEditableSprites(&count);
  if (count <= 0) {
    return;
  }
  const int currentIndex = GetRoadArtEditorSpriteIndex(editor->targetSprite);
  int nextIndex = (currentIndex + delta) % count;
  if (nextIndex < 0) {
    nextIndex += count;
  }
  editor->targetSprite = sprites[nextIndex];
}

inline RoadArtEditorField GetRoadArtEditorField(const RoadArtEditorState& editor) {
  return static_cast<RoadArtEditorField>(std::clamp(editor.fieldIndex, 0, 5));
}

inline const char* ToDisplayName(RoadArtEditorField field) {
  switch (field) {
    case RoadArtEditorField::kScalePercent:
      return "scale_percent";
    case RoadArtEditorField::kFootprintPercent:
      return "footprint_percent";
    case RoadArtEditorField::kOffsetX:
      return "offset_x";
    case RoadArtEditorField::kOffsetY:
      return "offset_y";
    case RoadArtEditorField::kAnchorX:
      return "anchor_x";
    case RoadArtEditorField::kAnchorY:
      return "anchor_y";
    default:
      return "unknown";
  }
}

inline int GetRoadArtEditorFieldValue(const RoadPieceArt& art, RoadArtEditorField field) {
  switch (field) {
    case RoadArtEditorField::kScalePercent:
      return art.scalePercent;
    case RoadArtEditorField::kFootprintPercent:
      return art.footprintPercent;
    case RoadArtEditorField::kOffsetX:
      return art.offsetX;
    case RoadArtEditorField::kOffsetY:
      return art.offsetY;
    case RoadArtEditorField::kAnchorX:
      return art.anchorX;
    case RoadArtEditorField::kAnchorY:
      return art.anchorY;
    default:
      return 0;
  }
}

inline void AdjustRoadArtEditorValue(RoadArtTuning* tuning, const RoadArtEditorState& editor,
                                     int delta) {
  RoadPieceArt* art = GetMutableRoadPieceArt(tuning, editor.targetSprite);
  switch (GetRoadArtEditorField(editor)) {
    case RoadArtEditorField::kScalePercent:
      art->scalePercent = std::clamp(art->scalePercent + delta, 1, 300);
      break;
    case RoadArtEditorField::kFootprintPercent:
      art->footprintPercent = std::clamp(art->footprintPercent + delta, 1, 300);
      break;
    case RoadArtEditorField::kOffsetX:
      art->offsetX += delta;
      break;
    case RoadArtEditorField::kOffsetY:
      art->offsetY += delta;
      break;
    case RoadArtEditorField::kAnchorX:
      art->anchorX = std::clamp(art->anchorX + delta, kRoadArtAnchorMin, kRoadArtAnchorMax);
      break;
    case RoadArtEditorField::kAnchorY:
      art->anchorY = std::clamp(art->anchorY + delta, kRoadArtAnchorMin, kRoadArtAnchorMax);
      break;
  }
}
