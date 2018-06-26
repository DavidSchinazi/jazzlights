#include "unisparks.hpp"
#include "loader.hpp"
#include "glrenderer.hpp"

#include <vector>
#include <sstream>
#include "cpptoml.hpp"
using namespace std;
using namespace unisparks;

Renderer& loadRenderer(Layout& layout) {
  static vector<unique_ptr<Renderer>> renderers;
  renderers.emplace_back(unique_ptr<Renderer>(new GLRenderer(layout)));
  return *renderers.back();
}


Layout& loadLayout(const cpptoml::table& cfg) {
  static vector<Point> pixelcoords;
  static vector<unique_ptr<Layout>> layouts;

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
       pixelcoords.push_back({coordscfg->at(i), coordscfg->at(i+1)});
     }
     PixelMap *m = new PixelMap(coordscfg->size()/3, &pixelcoords[begin]);
     layouts.emplace_back(unique_ptr<Layout>(m));
     return *layouts.back();
   }

  if (type.size() >= 6 && type.substr(0,6) == string("matrix")) {
    auto subtype = type.substr(6);
    
    auto size = cfg.get_array_of<int64_t>("size").value_or(
                  vector<int64_t>({5, 5}));
    auto origin = cfg.get_array_of<double>("origin")
                  .value_or(vector<double>({0, 0}));
    Matrix *m = new Matrix(size[0], size[1]);
    m->origin(origin[0], origin[1]);
    
    if (subtype == string("")) {
      // do nothing
    }
    else if (subtype == string("-zigzag")) {
      m->zigzag();
    }
    else {
      throw runtime_error("unknown matrix subtype '" + subtype + "'");
    }
    layouts.emplace_back(unique_ptr<Layout>(m));
    return *layouts.back();
  }

  throw runtime_error("unknown layout type '" + type + "'");
}

Player& loadPlayer(const cpptoml::table& cfg) {
  static Player player;
  static Udp network;

  player.connect(network);
  player.throttleFps(cfg.get_as<int64_t>("throttle-fps").value_or(60));

  auto strandscfg = cfg.get_table_array("strand");
  if (!strandscfg) {
    throw runtime_error("must define at least one strand");
  }

  for (const auto& strandcfg : *strandscfg) {
    auto layoutcfg = strandcfg->get_table("layout");
    if (!layoutcfg) {
      throw runtime_error("strand must specify layout");
    }
    Layout& layout = loadLayout(*layoutcfg);
    Renderer& renderer = loadRenderer(layout);
    player.addStrand(layout, renderer);
  }
  player.begin();

  return player;
}


Player& load(const char* file) {
  info("Loading %s...", file);
  try {
    auto config = cpptoml::parse_file(file);
    //cout << (*config) << endl;
    return loadPlayer(*config);
  } catch (const runtime_error& err) {
    fatal("Couldn't parse %s: %s", file, err.what());
  }
}
