#include "unisparks.hpp"

using namespace std;
using namespace unisparks;

Player player;
Matrix pixels(100, 100);

void dummyRender(int, uint8_t, uint8_t, uint8_t) {
}

int main(int, char**) {
  player.addStrand(pixels, &dummyRender);
  player.begin(timeMillis());
  uint32_t fps = 0;
  for(;;) {
    player.render(timeMillis());
    if (player.fps() != fps) {
      fps = player.fps();
      info("FPS: %u", fps);
    }
  }

  return 0;
}
