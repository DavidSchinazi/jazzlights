#include <getopt.h>

#include "jazzlights/layout/matrix.h"
#include "jazzlights/network/unix_udp.h"
#include "jazzlights/render/player.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

Player player;
Matrix pixels(100, 100);

class NoopRenderer : public Renderer {
 public:
  NoopRenderer() = default;
  void RenderPixel(size_t /*index*/, CRGB /*color*/) override {}
};

NoopRenderer noopRenderer;

int runMain(int argc, char** argv) {
  OptionalMicroseconds killTime;
  bool useNetwork = false;
  while (true) {
    int ch = getopt(argc, argv, "k:n");
    if (ch == -1) { break; }
    if (ch == 'k') { killTime = TimeMicros() + strtol(optarg, nullptr, 10) * kMicrosecondsPerSecond; }
    if (ch == 'n') { useNetwork = true; }
  }
  player.AddStrand(pixels, noopRenderer);
  if (useNetwork) { player.Connect(UnixUdpNetwork::get()); }
  player.Begin();
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
      player.GenerateFPSReport(&fpsCompute, &fpsWrites, &utilization, &timeSpentComputingThisEpoch, &epochDuration);
      jll_info("%u FPS %u%% %lld/%lldms", fpsCompute, utilization, MsForLogs(timeSpentComputingThisEpoch),
               MsForLogs(epochDuration));
      lastFpsEpochTime = currentTime;
    }
    player.Render();
  }
  return 0;
}

}  // namespace jazzlights

int main(int argc, char** argv) { return jazzlights::runMain(argc, argv); }
