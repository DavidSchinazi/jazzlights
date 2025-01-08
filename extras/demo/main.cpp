#include <memory>
#include <vector>

#include "glrenderer.h"
#include "gui.h"
#include "jazzlights/layouts/matrix.h"
#include "jazzlights/network/unix_udp.h"

namespace jazzlights {

int runMain(int /*argn*/, char** /*argv*/) {
  Matrix layout(/*w=*/400, /*h=*/300);
  GLRenderer renderer(layout);
  UnixUdpNetwork network;
  Player player;
  player.setBasePrecedence(30000);
  player.setPrecedenceGain(5000);
  player.addStrand(layout, renderer);
  player.setRandomizeLocalDeviceId(true);
  player.connect(&network);
  player.begin(timeMillis());

  return runGui("JazzLights Demo", player, player.bounds());
}

}  // namespace jazzlights

int main(int argc, char** argv) { return jazzlights::runMain(argc, argv); }
