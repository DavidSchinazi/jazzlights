#include "unisparks/util/loader.hpp"
#include "unisparks/layouts/matrix.hpp"
#include "unisparks/layouts/pixelmap.hpp"
#include "unisparks/layouts/transformed.hpp"
#include "unisparks/networks/udp.hpp"
#include <vector>
#include <sstream>
using namespace std;
using namespace unisparks;

Layout& Loader::loadLayout(const cpptoml::table& cfg) {
  static vector<Point> pixelcoords;
  static vector<unique_ptr<Layout>> layouts;

  // TODO: Fix this hack
  // The hack is to reserve a large number of points in order to prevent
  // re-allocating vector. We don't want realloc to happen because we're passing
  // pointers to internal vector memory to pixelmap layout.
  pixelcoords.reserve(100000);

  string type = cfg.get_as<string>("type").value_or("pixelmap");

  if (type == string("pixelmap")) {
    auto coordscfg = cfg.get_array_of<double>("coords");
    if (!coordscfg) {
      throw runtime_error("pixelmap must specify coords");
    }
    if (coordscfg->size() % 3 != 0) {
      stringstream msg;
      msg << "pixelmap coordinates must be a whole number of triplets, got "
          << coordscfg->size();
      throw runtime_error(msg.str());
    }
    size_t begin = pixelcoords.size();
    for (size_t i = 0; i < coordscfg->size(); i += 3) {
      pixelcoords.push_back({coordscfg->at(i), coordscfg->at(i + 1)});
    }
    PixelMap* m = new PixelMap(coordscfg->size() / 3, &pixelcoords[begin]);
    layouts.emplace_back(unique_ptr<Layout>(m));
    return *layouts.back();
  }

  if (type.size() >= 6 && type.substr(0, 6) == string("matrix")) {
    auto subtype = type.substr(6);

    auto size = cfg.get_array_of<int64_t>("size").value_or(
                  vector<int64_t>({5, 5}));
    auto origin = cfg.get_array_of<double>("origin")
                  .value_or(vector<double>({0, 0}));
    auto res = cfg.get_as<double>("resolution").value_or(60);
    Matrix* m = new Matrix(size[0], size[1]);
    m->origin(origin[0], origin[1]);
    m->resolution(res);
    if (subtype == string("")) {
      // do nothing
    } else if (subtype == string("-zigzag")) {
      m->zigzag();
    } else {
      throw runtime_error("unknown matrix subtype '" + subtype + "'");
    }
    layouts.emplace_back(unique_ptr<Layout>(m));
    return *layouts.back();
  }

  throw runtime_error("unknown layout type '" + type + "'");
}


void Loader::loadPlayer(Player& player, const cpptoml::table& cfg) {
  player.throttleFps(cfg.get_as<int64_t>("throttle-fps").value_or(60));

  ledr_ = cfg.get_as<Meters>("ledr").value_or(1.0/60.0);

  auto strandscfg = cfg.get_table_array("strand");
  if (!strandscfg) {
    throw runtime_error("must define at least one strand");
  }

  size_t strandidx = 0;
  for (const auto& strandcfg : *strandscfg) {
    auto layoutcfg = strandcfg->get_table("layout");
    if (!layoutcfg) {
      throw runtime_error("strand must specify layout");
    }
    Layout& layout = loadLayout(*layoutcfg);

    auto rendererscfg = strandcfg->get_table_array("renderers");
    if (rendererscfg) {
      for (const auto& rendcfg : *rendererscfg) {
        Renderer& renderer = loadRenderer(layout, *rendcfg, strandidx);
        player.addStrand(layout, renderer);
      }
    }
    strandidx++;
  }
}

static Udp network;

void Loader::load(const char* file, Player& player) {
  info("unisparks::Loader loading: %s", file);
  try {
    auto config = cpptoml::parse_file(file);
    //cout << (*config) << endl;
    loadPlayer(player, *config);
    player.connect(network);
  } catch (const runtime_error& err) {
    fatal("Couldn't parse %s: %s", file, err.what());
  }
}
