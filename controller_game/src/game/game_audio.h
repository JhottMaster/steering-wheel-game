#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "game_logic.h"
#include "raylib.h"

struct GameAudio {
  Music background = {};
  Music engine = {};
  Sound coin = {};
  Sound krakenGrowl = {};
  Sound krakenSink = {};
  bool ready = false;
  bool backgroundLoaded = false;
  bool engineLoaded = false;
  bool coinLoaded = false;
  bool krakenGrowlLoaded = false;
  bool krakenSinkLoaded = false;
  int lastScore = 0;
  int lastKrakensCollected = 0;
  bool krakenWasNearby = false;
};

namespace game_audio_detail {
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

inline std::string FindSoundPath(const char* filename) {
  return FindAssetPath("assets/sounds", filename);
}

inline std::string FindFirstSoundPath(std::initializer_list<const char*> filenames) {
  for (const char* filename : filenames) {
    const std::string candidate = FindSoundPath(filename);
    if (FileExists(candidate.c_str())) {
      return candidate;
    }
  }

  return FindSoundPath(*filenames.begin());
}

inline Sound LoadOptionalSound(std::initializer_list<const char*> filenames, bool* loaded) {
  const std::string path = FindFirstSoundPath(filenames);
  if (!FileExists(path.c_str())) {
    *loaded = false;
    return {};
  }

  Sound sound = LoadSound(path.c_str());
  *loaded = sound.stream.buffer != nullptr;
  return sound;
}
}  // namespace game_audio_detail

inline GameAudio LoadGameAudio() {
  GameAudio audio;
  if (!IsAudioDeviceReady()) {
    return audio;
  }

  audio.ready = true;
  audio.background =
      LoadMusicStream(game_audio_detail::FindSoundPath("carpet_cruise_loop.wav").c_str());
  audio.backgroundLoaded = audio.background.stream.buffer != nullptr;
  audio.engine = LoadMusicStream(
      game_audio_detail::FindFirstSoundPath({"engine_large.ogg", "toy_engine_loop.wav"}).c_str());
  audio.engineLoaded = audio.engine.stream.buffer != nullptr;
  audio.coin = LoadSound(game_audio_detail::FindSoundPath("coin_chime.wav").c_str());
  audio.coinLoaded = audio.coin.stream.buffer != nullptr;
  audio.krakenGrowl = game_audio_detail::LoadOptionalSound(
      {"silent_beast_growl.mp3", "kraken_growl.wav"}, &audio.krakenGrowlLoaded);
  audio.krakenSink = game_audio_detail::LoadOptionalSound(
      {"splash1.wav", "kraken_sink.wav"}, &audio.krakenSinkLoaded);

  if (audio.backgroundLoaded) {
    audio.background.looping = true;
    SetMusicVolume(audio.background, 0.28f);
    PlayMusicStream(audio.background);
  }
  if (audio.engineLoaded) {
    audio.engine.looping = true;
    SetMusicVolume(audio.engine, 0.0f);
    PlayMusicStream(audio.engine);
  }
  if (audio.coinLoaded) {
    SetSoundVolume(audio.coin, 0.72f);
  }
  if (audio.krakenGrowlLoaded) {
    SetSoundVolume(audio.krakenGrowl, 0.46f);
  }
  if (audio.krakenSinkLoaded) {
    SetSoundVolume(audio.krakenSink, 0.58f);
  }

  return audio;
}

inline void UnloadGameAudio(GameAudio* audio) {
  if (audio->backgroundLoaded) {
    UnloadMusicStream(audio->background);
  }
  if (audio->engineLoaded) {
    UnloadMusicStream(audio->engine);
  }
  if (audio->coinLoaded) {
    UnloadSound(audio->coin);
  }
  if (audio->krakenGrowlLoaded) {
    UnloadSound(audio->krakenGrowl);
  }
  if (audio->krakenSinkLoaded) {
    UnloadSound(audio->krakenSink);
  }
}

inline void UpdateGameAudio(GameAudio* audio, const GameState& game, bool gameModeActive) {
  if (!audio->ready) {
    return;
  }

  if (audio->backgroundLoaded) {
    UpdateMusicStream(audio->background);
    SetMusicVolume(audio->background, gameModeActive ? 0.28f : 0.10f);
  }

  if (audio->engineLoaded) {
    UpdateMusicStream(audio->engine);
    const float speedUnit = std::clamp(std::fabs(game.carSpeed) / kGameManualMaxSpeed, 0.0f, 1.0f);
    const float enginePresence = speedUnit * speedUnit;
    SetMusicVolume(audio->engine, gameModeActive ? 0.06f + enginePresence * 0.30f : 0.0f);
    SetMusicPitch(audio->engine, 0.84f + speedUnit * 1.02f);
  }

  if (audio->coinLoaded && game.score > audio->lastScore) {
    PlaySound(audio->coin);
  }
  audio->lastScore = game.score;

  if (gameModeActive && audio->krakenGrowlLoaded && game.krakenNearby &&
      !audio->krakenWasNearby) {
    PlaySound(audio->krakenGrowl);
  }
  audio->krakenWasNearby = gameModeActive && game.krakenNearby;

  if (audio->krakenSinkLoaded && game.krakensCollected > audio->lastKrakensCollected) {
    PlaySound(audio->krakenSink);
  }
  audio->lastKrakensCollected = game.krakensCollected;
}
