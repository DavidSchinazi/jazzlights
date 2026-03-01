#include <getopt.h>

#include "jazzlights/layout/matrix.h"
#include "jazzlights/network/unix_udp.h"
#include "jazzlights/player.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

Player player;
Matrix pixels(100, 100);

class NoopRenderer : public Renderer {
 public:
  NoopRenderer() = default;
  void renderPixel(size_t /*index*/, CRGB /*color*/) override {}
};

NoopRenderer noopRenderer;

int runMain(int argc, char** argv) {
  int killTime = 0;
  bool useNetwork = false;
  while (true) {
    int ch = getopt(argc, argv, "k:n");
    if (ch == -1) { break; }
    if (ch == 'k') { killTime = strtol(optarg, nullptr, 10) * 1000; }
    if (ch == 'n') { useNetwork = true; }
  }
  player.addStrand(pixels, noopRenderer);
  if (useNetwork) { player.connect(UnixUdpNetwork::get()); }
  player.begin();
  Milliseconds lastFpsEpochTime = 0;
  while (true) {
    const Milliseconds currentTime = timeMillis();
    if (killTime > 0 && currentTime > killTime) {
      jll_info("Kill time reached, exiting.");
      exit(0);
    }
    if (currentTime - lastFpsEpochTime > 1000) {
      uint16_t fpsCompute;
      uint16_t fpsWrites;
      uint8_t utilization = 0;
      Milliseconds timeSpentComputingThisEpoch;
      Milliseconds epochDuration;
      player.GenerateFPSReport(&fpsCompute, &fpsWrites, &utilization, &timeSpentComputingThisEpoch, &epochDuration);
      jll_info("%u FPS %u%% %u/%ums", fpsCompute, utilization, timeSpentComputingThisEpoch, epochDuration);
      lastFpsEpochTime = currentTime;
    }
    player.render(currentTime);
  }
  return 0;
}

}  // namespace jazzlights

int main(int argc, char** argv) { return jazzlights::runMain(argc, argv); }
