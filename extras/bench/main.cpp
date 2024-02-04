#include "jazzlights/layouts/matrix.h"
#include "jazzlights/player.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

Player player;
Matrix pixels(100, 100);

class NoopRenderer : public Renderer {
 public:
  NoopRenderer() = default;

  void renderPixel(size_t /*index*/, Color color) override {
    RgbaColor rgba = color.asRgba();
    (void)rgba.red;
    (void)rgba.green;
    (void)rgba.blue;
  }
};

NoopRenderer noopRenderer;

int runMain() {
  player.addStrand(pixels, noopRenderer);
  player.begin(timeMillis());
  uint32_t fps = 0;
  while (true) {
    player.render(timeMillis());
    if (player.fps() != fps) {
      fps = player.fps();
      jll_info("FPS: %u", fps);
    }
  }
  return 0;
}

}  // namespace jazzlights

int main(int /*argc*/, char** /*argv*/) { return jazzlights::runMain(); }
