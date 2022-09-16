#include "glrenderer.h"
#include "gui.h"

#include "jazzlights/networks/udp.h"
#include "jazzlights/util/loader.h"

#include <vector>
#include <memory>

namespace jazzlights {

class DemoLoader : public Loader {
  Renderer& loadRenderer(const Layout& layout, const cpptoml::table&, int strandidx) override {
    static std::vector<std::unique_ptr<Renderer>> renderers;
    static int laststrandidx = -1;
    if (laststrandidx == strandidx) {
      return *renderers.back();
    }
    laststrandidx = strandidx;
    renderers.emplace_back(std::make_unique<GLRenderer>(layout, ledRadius()));
    return *renderers.back();
  }
};

int runMain(int argn, char** argv) {
  if (argn<2) {
    printf("Usage: %s <config>\n", argv[0]);
    exit(-1);
  }
  bool fullscreen = false;
  const char* config = argv[1];

  Player player;
  UnixUdpNetwork network;

  player.setBasePrecedence(30000);
  player.setPrecedenceGain(5000);
  DemoLoader().load(config, player);
  player.setRandomizeLocalDeviceId(true);
  player.connect(&network);
  player.begin(timeMillis());

  return runGui("JazzLights Demo", player, player.bounds(), fullscreen);
}

}  // namespace jazzlights

int main(int argc, char** argv) {
  return jazzlights::runMain(argc, argv);
}
