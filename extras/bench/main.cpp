#include "jazzlights/layouts/matrix.h"
#include "jazzlights/player.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

Player player;
Matrix pixels(100, 100);

void noopRender(int, uint8_t, uint8_t, uint8_t) {}

int runMain() {
  player.addStrand(pixels, &noopRender);
  player.begin(timeMillis());
  uint32_t fps = 0;
  while (true) {
    player.render(timeMillis());
    if (player.fps() != fps) {
      fps = player.fps();
      info("FPS: %u", fps);
    }
  }
  return 0;
}

}  // namespace jazzlights

int main(int /*argc*/, char** /*argv*/) {
  return jazzlights::runMain();
}
