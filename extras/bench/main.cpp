#include "jazzlights/layouts/matrix.h"
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

int runMain() {
  player.addStrand(pixels, noopRenderer);
  player.begin(timeMillis());
  Milliseconds lastFpsEpochTime = 0;
  while (true) {
    const Milliseconds currentTime = timeMillis();
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

int main(int /*argc*/, char** /*argv*/) { return jazzlights::runMain(); }
