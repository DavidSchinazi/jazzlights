#ifndef JL_EFFECTS_SOLID_H
#define JL_EFFECTS_SOLID_H

#include "jazzlights/effects/functional.h"

namespace jazzlights {

inline FunctionalEffect solid(Color color, const std::string& name) {
  return effect(name,
                [color](const Frame& /*frame*/) { return [color](const Pixel& /*pt*/) -> Color { return color; }; });
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_SOLID_H
