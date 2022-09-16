#ifndef JAZZLIGHTS_UTIL_LOADER_HPP
#define JAZZLIGHTS_UTIL_LOADER_HPP

#include "jazzlights/player.h"
#include "jazzlights/util/cpptoml.h"

namespace jazzlights {

class Loader {
 public:
  void load(const char* file, Player& player);

  Meters ledRadius() const {
    return ledr_;
  }

 private:
  void loadPlayer(Player& player, const cpptoml::table& cfg);
  Layout& loadLayout(const cpptoml::table& cfg);
  virtual Renderer& loadRenderer(const Layout& lo, const cpptoml::table& cfg, int strandidx) = 0;

  Meters ledr_;
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_UTIL_LOADER_HPP
