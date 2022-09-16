#include "jazzlights/util/loader.hpp"
#include "jazzlights/layouts/matrix.hpp"
#include "jazzlights/layouts/pixelmap.hpp"
#include "jazzlights/networks/udp.hpp"

#include <vector>
#include <sstream>

namespace jazzlights {

Layout& Loader::loadLayout(const cpptoml::table& cfg) {
  static std::vector<Point> pixelcoords;
  static std::vector<std::unique_ptr<Layout>> layouts;

  // We should fix this hack
  // The hack is to reserve a large number of points in order to prevent
  // re-allocating vector. We don't want realloc to happen because we're passing
  // pointers to internal vector memory to pixelmap layout.
  pixelcoords.reserve(100000);

  std::string type = cfg.get_as<std::string>("type").value_or("pixelmap");

  if (type == "pixelmap") {
    auto coordscfg = cfg.get_array_of<double>("coords");
    if (!coordscfg) {
      throw std::runtime_error("pixelmap must specify coords");
    }
    if (coordscfg->size() % 3 != 0) {
      std::stringstream msg;
      msg << "pixelmap coordinates must be a whole number of triplets, got "
          << coordscfg->size();
      throw std::runtime_error(msg.str());
    }
    const size_t begin = pixelcoords.size();
    const bool backwards = cfg.get_as<bool>("backwards").value_or(false);
    if (backwards) {
      for (size_t i = coordscfg->size() - 3; ; i -= 3) {
        pixelcoords.push_back({coordscfg->at(i), coordscfg->at(i + 1)});
        if (i == 0) { break; }
      }
    } else {
      for (size_t i = 0; i < coordscfg->size(); i += 3) {
        pixelcoords.push_back({coordscfg->at(i), coordscfg->at(i + 1)});
      }
    }
    layouts.emplace_back(std::make_unique<PixelMap>(coordscfg->size() / 3, &pixelcoords[begin]));
    return *layouts.back();
  }

  if (type.size() >= 6 && type.substr(0, 6) == "matrix") {
    auto subtype = type.substr(6);

    auto size = cfg.get_array_of<int64_t>("size").value_or(
                  std::vector<int64_t>({5, 5}));
    auto origin = cfg.get_array_of<double>("origin")
                  .value_or(std::vector<double>({0, 0}));
    auto res = cfg.get_as<double>("resolution").value_or(60);
    auto m = std::make_unique<Matrix>(size[0], size[1]);
    m->origin(origin[0], origin[1]);
    m->resolution(res);
    if (subtype == "") {
      // do nothing
    } else if (subtype == "-zigzag") {
      m->zigzag();
    } else {
      throw std::runtime_error("unknown matrix subtype '" + subtype + "'");
    }
    layouts.emplace_back(std::move(m));
    return *layouts.back();
  }

  throw std::runtime_error("unknown layout type '" + type + "'");
}


void Loader::loadPlayer(Player& player, const cpptoml::table& cfg) {
  ledr_ = cfg.get_as<Meters>("ledr").value_or(1.0/60.0);
  cpptoml::option<Precedence> basePrecedence = cfg.get_as<Precedence>("baseprecedence");
  if (basePrecedence) {
    player.setBasePrecedence(*basePrecedence);
  }
  cpptoml::option<Precedence> precedenceGain = cfg.get_as<Precedence>("precedencegain");
  if (precedenceGain) {
    player.setPrecedenceGain(*precedenceGain);
  }

  auto strandscfg = cfg.get_table_array("strand");
  if (!strandscfg) {
    throw std::runtime_error("must define at least one strand");
  }

  size_t strandidx = 0;
  for (const auto& strandcfg : *strandscfg) {
    auto layoutcfg = strandcfg->get_table("layout");
    if (!layoutcfg) {
      throw std::runtime_error("strand must specify layout");
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

void Loader::load(const char* file, Player& player) {
  info("jazzlights::Loader loading: %s", file);
  try {
    auto config = cpptoml::parse_file(file);
    //cout << (*config) << endl;
    loadPlayer(player, *config);
  } catch (const std::runtime_error& err) {
    fatal("Couldn't parse %s: %s", file, err.what());
  }
}

}  // namespace jazzlights
