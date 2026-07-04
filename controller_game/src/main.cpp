#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include "platform/platform_windows.h"
#else
#include "platform/platform_linux.h"
#endif

#include "app/app_mode.h"
#include "app/app_renderer.h"
#include "app/app_runtime.h"
#include "app/frame_input.h"
#include "app/hotkeys.h"
#include "app/menu_flow.h"
#include "app/performance_log.h"
#include "app/server_discovery.h"
#include "game/game_audio.h"
#include "game/game_logic.h"
#include "raylib.h"

namespace {
constexpr int kUdpPort = 4210;
std::string JoinLocalIps(const std::vector<std::string>& addresses) {
  std::ostringstream joined;
  for (size_t i = 0; i < addresses.size(); ++i) {
    if (i > 0) {
      joined << ", ";
    }
    joined << addresses[i];
  }
  return joined.str();
}

}  // namespace

int main() {
  platform::UdpReceiver receiver;
  platform::UdpBroadcaster broadcaster;
  const bool udpReady = receiver.Open(kUdpPort);
  const bool broadcastReady = broadcaster.Open();
  const std::string localIpText = JoinLocalIps(platform::GetLocalIpv4Addresses());

  SetConfigFlags(platform::GetWindowConfigFlags());
  InitWindow(platform::kWindowWidth, platform::kWindowHeight, platform::kWindowTitle);
  platform::ApplyPostWindowInit();
  SetTargetFPS(60);
  InitAudioDevice();

  AppRuntime app = LoadAppRuntime();

  while (!app.shouldQuit && !WindowShouldClose()) {
    const auto frameStartTime = PerfClock::now();
    auto inputStartTime = frameStartTime;
    if (platform::ShouldToggleFullscreen()) {
      ToggleFullscreen();
    }
    if (!app.pauseMenu.active && IsKeyPressed(KEY_T)) {
      app.appMode = app.appMode == AppMode::kGame ? AppMode::kHardwareTest : AppMode::kGame;
    }
    HandleGameHotkeys(app.pauseMenu.active, app.appMode, &app.game, &app.roadArtEditor,
                      &app.roadArtTuning, app.roadArtTuningPath, &app.cameraZoomScale);
    HandleHardwareTestHotkeys(app.pauseMenu.active, app.appMode, &app.displayAxis);

    const FrameInput input = ReadFrameInput(&app, &receiver);
    const auto inputEndTime = PerfClock::now();

    UpdateServerDiscoveryBeacon(&app.discovery, &broadcaster, broadcastReady,
                                input.hasFreshPackets, app.lastPacketTime, input.now);
    UpdateMenusAndOverlays(&app, input);

    const auto updateStartTime = PerfClock::now();
    if (app.appMode == AppMode::kGame && !app.pauseMenu.active) {
      UpdateGame(&app.game, input.gameSteeringInput, input.gameButtons, input.dt, &app.city);
    }
    const auto updateEndTime = PerfClock::now();
    const auto audioStartTime = updateEndTime;
    UpdateGameAudio(&app.gameAudio, app.game,
                    app.appMode == AppMode::kGame && !app.pauseMenu.active);
    const auto audioEndTime = PerfClock::now();

    const auto drawStartTime = PerfClock::now();
    DrawApp(app, input, udpReady, localIpText);
    const auto frameEndTime = PerfClock::now();
    AddPerformanceSample(
        &app.performance, ElapsedMilliseconds(frameStartTime, frameEndTime),
        static_cast<double>(input.dt) * 1000.0,
        ElapsedMilliseconds(inputStartTime, inputEndTime),
        ElapsedMilliseconds(updateStartTime, updateEndTime),
        ElapsedMilliseconds(audioStartTime, audioEndTime),
        ElapsedMilliseconds(drawStartTime, frameEndTime));
    MaybeLogPerformance(&app.performance, app.appMode, app.pauseMenu.active);
  }

  UnloadAppRuntime(&app);
  if (IsAudioDeviceReady()) {
    CloseAudioDevice();
  }
  CloseWindow();
  return 0;
}
