#include "unisparks.hpp"

using namespace std;
using namespace unisparks;

Player player;
Matrix pixels(100, 100);

void dummyRender(int, uint8_t, uint8_t, uint8_t) {
}

int main(int, char**) {
  player
    .addStrand(pixels, &dummyRender)
    .throttleFps(0)
  ;
  player.begin();
  int fps = -1;
  for(;;) {
    player.render(millis());
    if (player.fps() != fps) {
      info("FPS: %d", fps);
      fps = player.fps();
    }
  }

  return 0;
}
