#ifndef UNISPARKS_EFFECTS_SOLID_H
#define UNISPARKS_EFFECTS_SOLID_H
#include "unisparks/effects/functional.hpp"

namespace unisparks {

auto solid = [](Color color, const std::string& name) {
  return effect(name, [ = ](const Frame& /*frame*/) {
    return [ = ](const Pixel& /*pt*/) -> Color {
      return color;
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_SOLID_H */
