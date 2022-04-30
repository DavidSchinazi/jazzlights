#include <vector>
#include <sstream>
#include "tgloader.hpp"
#include "renderers/pixelpusher.hpp"
#include "renderers/openpixel.hpp"
#include "unisparks.hpp"
using namespace std;
using namespace unisparks;


class TGLoader : public unisparks::Loader {
  unisparks::Renderer& loadRenderer(const unisparks::Layout& layout,
                                    const cpptoml::table&, int strandidx) override;
};

Renderer& TGLoader::loadRenderer(const unisparks::Layout&,
                                 const cpptoml::table& cfg, int strandidx) {

  static vector<unique_ptr<Renderer>> renderers;
  auto typecfg = cfg.get_as<string>("type");
  if (!typecfg) {
    throw runtime_error("must specify renderer type");
  }
  auto type = typecfg->c_str();

  if (!strcmp(type, "pixelpusher")) {
    auto addrcfg = cfg.get_as<string>("addr");
    if (!addrcfg) {
      throw runtime_error("must specify pixelpusher address");
    }
    auto addr = addrcfg->c_str();
    auto strip = cfg.get_as<int64_t>("strip").value_or(strandidx);
    auto port = cfg.get_as<int64_t>("port").value_or(7331);
    auto throttle = cfg.get_as<int64_t>("throttle").value_or(1000 / 30);
    auto controller = cfg.get_as<int64_t>("controller").value_or(0);
    auto group = cfg.get_as<int64_t>("group").value_or(0);
 
    renderers.emplace_back(unique_ptr<Renderer>(new PixelPusher(strdup(addr), port,
                           strip, throttle, controller, group)));
  } else if (!strcmp(type, "openpixel")) {
    auto addrcfg = cfg.get_as<string>("addr");
    if (!addrcfg) {
      throw runtime_error("must specify openpixel address");
    }
    auto addr = addrcfg->c_str();
    auto channel = cfg.get_as<int64_t>("channel").value_or(0);
    auto port = cfg.get_as<int64_t>("port").value_or(5000);
    renderers.emplace_back(unique_ptr<Renderer>(new OpenPixelWriter(addr, port,
                           channel)));
  }
  return *renderers.back();
}

void load(const char *file, unisparks::Player& player) {
  for(;;) {
    try {
      info("Loading %s...", file);
      TGLoader().load(file, player);
      return;
    } catch (const runtime_error& err) {
      error("Couldn't parse %s: %s", file, err.what());
      delay(2000);
    }
  }
}
