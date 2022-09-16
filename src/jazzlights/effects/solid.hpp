#ifndef JAZZLIGHTS_EFFECTS_SOLID_H
#define JAZZLIGHTS_EFFECTS_SOLID_H
#include "jazzlights/effects/functional.hpp"

namespace jazzlights {

auto solid = [](Color color, const std::string& name) {
  return effect(name, [ = ](const Frame& /*frame*/) {
    return [ = ](const Pixel& /*pt*/) -> Color {
      return color;
    };
  });
};

}  // namespace jazzlights
#endif  // JAZZLIGHTS_EFFECTS_SOLID_H
