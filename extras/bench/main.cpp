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
  std::optional<Microseconds> killTime;
  bool useNetwork = false;
  while (true) {
    int ch = getopt(argc, argv, "k:n");
    if (ch == -1) { break; }
    if (ch == 'k') { killTime = timeMicros() + strtol(optarg, nullptr, 10) * kMicrosecondsPerSecond; }
    if (ch == 'n') { useNetwork = true; }
  }
  player.addStrand(pixels, noopRenderer);
  if (useNetwork) { player.connect(UnixUdpNetwork::get()); }
  player.begin();
  Microseconds lastFpsEpochTime = 0;
  while (true) {
    const Microseconds currentTime = timeMicros();
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
      jll_info("%u FPS %u%% %lld/%lldms", fpsCompute, utilization,
               static_cast<long long>(timeSpentComputingThisEpoch / kMicrosecondsPerMillisecond),
               static_cast<long long>(epochDuration / kMicrosecondsPerMillisecond));
      lastFpsEpochTime = currentTime;
    }
    player.render();
  }
  return 0;
}

}  // namespace jazzlights

int main(int argc, char** argv) { return jazzlights::runMain(argc, argv); }
