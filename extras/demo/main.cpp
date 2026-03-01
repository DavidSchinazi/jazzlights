#include <getopt.h>

#include <memory>
#include <vector>

#include "glrenderer.h"
#include "gui.h"
#include "jazzlights/layout/matrix.h"
#include "jazzlights/network/unix_udp.h"

namespace jazzlights {

int runMain(int argc, char** argv) {
  int killTime = 0;
  bool startLooping = false;
  bool shouldSetPattern = false;
  PatternBits pattern = 0;
  while (true) {
    int ch = getopt(argc, argv, "k:p:l");
    if (ch == -1) { break; }
    if (ch == 'k') { killTime = strtol(optarg, nullptr, 10) * 1000; }
    if (ch == 'p') {
      shouldSetPattern = true;
      pattern = strtoll(optarg, nullptr, 16);
    }
    if (ch == 'l') { startLooping = true; }
    if (ch == '?') { return 1; }
  }
  Matrix layout(/*w=*/400, /*h=*/300);
  GLRenderer renderer(layout);
  Player player;
  player.setBasePrecedence(30000);
  player.setPrecedenceGain(5000);
  player.addStrand(layout, renderer);
  player.setRandomizeLocalDeviceId(true);
  player.connect(UnixUdpNetwork::get());
  player.begin();
  if (startLooping) { player.loopOne(timeMillis()); }
  if (shouldSetPattern) { player.setPattern(pattern, timeMillis()); }

  return runGui("JazzLights Demo", player, player.bounds(), /*fullscreen=*/false, killTime);
}

}  // namespace jazzlights

int main(int argc, char** argv) { return jazzlights::runMain(argc, argv); }
