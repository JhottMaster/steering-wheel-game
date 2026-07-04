#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>

#include "app_mode.h"

constexpr double kPerformanceLogIntervalSeconds = 2.0;

using PerfClock = std::chrono::steady_clock;

struct PerfBucket {
  double totalMs = 0.0;
  double maxMs = 0.0;

  void Add(double milliseconds) {
    totalMs += milliseconds;
    maxMs = std::max(maxMs, milliseconds);
  }

  double Average(int sampleCount) const {
    return sampleCount > 0 ? totalMs / static_cast<double>(sampleCount) : 0.0;
  }
};

struct PerformanceWindow {
  PerfBucket frameCpu;
  PerfBucket dt;
  PerfBucket input;
  PerfBucket update;
  PerfBucket audio;
  PerfBucket draw;
  int frames = 0;
  PerfClock::time_point startedAt = PerfClock::now();
};

inline double ElapsedMilliseconds(PerfClock::time_point start, PerfClock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

inline void AddPerformanceSample(PerformanceWindow* performance, double frameCpuMs, double dtMs,
                                 double inputMs, double updateMs, double audioMs, double drawMs) {
  performance->frameCpu.Add(frameCpuMs);
  performance->dt.Add(dtMs);
  performance->input.Add(inputMs);
  performance->update.Add(updateMs);
  performance->audio.Add(audioMs);
  performance->draw.Add(drawMs);
  ++performance->frames;
}

inline void MaybeLogPerformance(PerformanceWindow* performance, AppMode appMode,
                                bool pauseMenuActive) {
  const auto now = PerfClock::now();
  const double elapsedSeconds =
      std::chrono::duration<double>(now - performance->startedAt).count();
  if (elapsedSeconds < kPerformanceLogIntervalSeconds || performance->frames <= 0) {
    return;
  }

  const double averageFrameCpuMs = performance->frameCpu.Average(performance->frames);
  const double averageDtMs = performance->dt.Average(performance->frames);
  const double averageInputMs = performance->input.Average(performance->frames);
  const double averageUpdateMs = performance->update.Average(performance->frames);
  const double averageAudioMs = performance->audio.Average(performance->frames);
  const double averageDrawMs = performance->draw.Average(performance->frames);
  const double averageOtherMs =
      std::max(0.0, averageFrameCpuMs -
                        (averageInputMs + averageUpdateMs + averageAudioMs + averageDrawMs));

  std::printf(
      "[perf] mode=%s pause=%s frames=%d dt(avg/max)=%.2f/%.2f ms cpu(avg/max)=%.2f/%.2f ms "
      "input=%.2f update=%.2f audio=%.2f draw=%.2f other=%.2f\n",
      appMode == AppMode::kGame ? "game" : "hardware", pauseMenuActive ? "yes" : "no",
      performance->frames, averageDtMs, performance->dt.maxMs, averageFrameCpuMs,
      performance->frameCpu.maxMs, averageInputMs, averageUpdateMs, averageAudioMs,
      averageDrawMs, averageOtherMs);

  *performance = PerformanceWindow{};
}
