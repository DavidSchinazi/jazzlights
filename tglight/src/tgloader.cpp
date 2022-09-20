#include "tgloader.h"

#include <unistd.h>

#include <sstream>
#include <vector>

#include "jazzlights/util/loader.h"
#include "renderers/pixelpusher.h"

namespace jazzlights {

class TGLoader : public Loader {
  Renderer& loadRenderer(const Layout& layout, const cpptoml::table&, int strandidx) override;
};

Renderer& TGLoader::loadRenderer(const Layout&, const cpptoml::table& cfg, int strandidx) {
  static std::vector<std::unique_ptr<Renderer>> renderers;
  auto typecfg = cfg.get_as<std::string>("type");
  if (!typecfg) { throw std::runtime_error("must specify renderer type"); }
  auto type = typecfg->c_str();

  if (!strcmp(type, "pixelpusher")) {
    auto addrcfg = cfg.get_as<std::string>("addr");
    if (!addrcfg) { throw std::runtime_error("must specify pixelpusher address"); }
    auto addr = addrcfg->c_str();
    auto strip = cfg.get_as<int64_t>("strip").value_or(strandidx);
    auto port = cfg.get_as<int64_t>("port").value_or(7331);
    auto throttle = cfg.get_as<int64_t>("throttle").value_or(1000 / 30);
    auto controller = cfg.get_as<int64_t>("controller").value_or(0);
    auto group = cfg.get_as<int64_t>("group").value_or(0);

    renderers.emplace_back(std::make_unique<PixelPusher>(strdup(addr), port, strip, throttle, controller, group));
  }
  return *renderers.back();
}

void load(const char* file, Player& player) {
  while (true) {
    try {
      info("Loading %s...", file);
      TGLoader().load(file, player);
      return;
    } catch (const std::runtime_error& err) {
      error("Couldn't parse %s: %s", file, err.what());
      sleep(2);
    }
  }
}

}  // namespace jazzlights
