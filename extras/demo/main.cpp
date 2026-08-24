#include <getopt.h>

#include <memory>
#include <vector>

#include "glrenderer.h"
#include "gui.h"
#include "jazzlights/layout/matrix.h"
#include "jazzlights/network/unix_udp.h"

namespace jazzlights {

int runMain(int argc, char** argv) {
  OptionalMicroseconds killTime;
  bool startLooping = false;
  bool shouldSetPattern = false;
  PatternBits pattern = 0;
  while (true) {
    int ch = getopt(argc, argv, "k:p:l");
    if (ch == -1) { break; }
    if (ch == 'k') { killTime = TimeMicros() + strtol(optarg, nullptr, 10) * kMicrosecondsPerSecond; }
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
  player.SetBasePrecedence(30000);
  player.SetPrecedenceGain(5000);
  player.AddStrand(layout, renderer);
  player.SetRandomizeLocalDeviceId(true);
  player.Connect(UnixUdpNetwork::Get());
  player.Begin();
  if (startLooping) { player.LoopOne(); }
  if (shouldSetPattern) { player.SetPattern(pattern); }

  return runGui("JazzLights Demo", player, player.bounds(), /*fullscreen=*/false, killTime);
}

}  // namespace jazzlights

int main(int argc, char** argv) { return jazzlights::runMain(argc, argv); }
