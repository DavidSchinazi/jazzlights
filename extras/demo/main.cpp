#include "glrenderer.hpp"
#include "gui.hpp"
#include "unisparks.hpp"
#include <vector>
#include <memory>
using namespace unisparks;
using std::vector;
using std::unique_ptr;

class DemoLoader : public Loader {
  Renderer& loadRenderer(const Layout& layout, const cpptoml::table&, int strandidx) override {
    static vector<unique_ptr<Renderer>> renderers;
    static int laststrandidx = -1;
    if (laststrandidx == strandidx) {
      return *renderers.back();
    }
    laststrandidx = strandidx;
    renderers.emplace_back(unique_ptr<Renderer>(new GLRenderer(layout, ledRadius())));
    return *renderers.back();
  }
};

int main(int argn, char** argv) {
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

  return runGui("Unisparks Demo", player, player.bounds(), fullscreen);
}
