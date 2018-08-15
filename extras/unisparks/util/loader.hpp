#ifndef UNISPARKS_UTIL_LOADER_HPP
#define UNISPARKS_UTIL_LOADER_HPP
#include "unisparks/player.hpp"
#include "unisparks/util/cpptoml.hpp"

namespace unisparks {

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

}
#endif /* UNISPARKS_UTIL_LOADER_HPP */
