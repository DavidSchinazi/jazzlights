#include <getopt.h>

#include "jazzlights/layout/matrix.h"
#include "jazzlights/network/manager.h"
#include "jazzlights/network/unix_udp.h"
#include "jazzlights/render/player.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

NetworkManager gNetworkManager;
Player gPlayer(gNetworkManager);
Matrix gPixels(100, 100);

class NoopRenderer : public Renderer {
 public:
  NoopRenderer() = default;
  void RenderPixel(size_t /*index*/, CRGB /*color*/) override {}
};

NoopRenderer gNoopRenderer;

int RunMain(int argc, char** argv) {
  OptionalMicroseconds killTime;
  bool useNetwork = false;
  while (true) {
    int ch = getopt(argc, argv, "k:n");
    if (ch == -1) { break; }
    if (ch == 'k') { killTime = TimeMicros() + strtol(optarg, nullptr, 10) * kMicrosecondsPerSecond; }
    if (ch == 'n') { useNetwork = true; }
  }
  gPlayer.AddStrand(gPixels, gNoopRenderer);
  if (useNetwork) { gNetworkManager.Connect(UnixUdpNetwork::Get()); }
  gPlayer.Begin();
  Microseconds lastFpsEpochTime = 0;
  while (true) {
    const Microseconds currentTime = TimeMicros();
    if (killTime && currentTime > *killTime) {
      jll_info("Kill time reached, exiting.");
      exit(0);
    }
    if (currentTime - lastFpsEpochTime > kMicrosecondsPerSecond) {
      uint16_t fpsCompute;
      uint16_t fpsWrites;
      uint8_t utilization = 0;
      Microseconds timeSpentComputingThisEpoch;
      Microseconds epochDuration;
      gPlayer.GenerateFPSReport(&fpsCompute, &fpsWrites, &utilization, &timeSpentComputingThisEpoch, &epochDuration);
      jll_info("%u FPS %u%% %lld/%lldms", fpsCompute, utilization, MsForLogs(timeSpentComputingThisEpoch),
               MsForLogs(epochDuration));
      lastFpsEpochTime = currentTime;
    }
    gPlayer.Render();
  }
  return 0;
}

}  // namespace jazzlights

int main(int argc, char** argv) { return jazzlights::RunMain(argc, argv); }
