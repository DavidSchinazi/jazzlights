#include "unisparks.hpp"

using namespace std;
using namespace unisparks;

Player player;
Matrix pixels(100, 100);

void dummyRender(int, uint8_t, uint8_t, uint8_t) {
}

int main(int, char**) {
  auto renderer = SimpleRenderer(dummyRender);
  Strand strand = {&pixels, &renderer};
  PlayerOptions opts;
  opts.strands = &strand;
  opts.strandCount = 1;
  opts.throttleFps = 0;
  opts.preferredEffectDuration = 10*ONE_SECOND;
  player.begin(opts);
  player.play(8);
  int fps = -1;
  for(;;) {
    player.render();
    if (player.fps() != fps) {
      info("FPS: %d", fps);
      fps = player.fps();
    }
  }

  return 0;
}
