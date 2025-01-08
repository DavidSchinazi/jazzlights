#ifndef JL_EFFECTS_SOLID_H
#define JL_EFFECTS_SOLID_H

#include "jazzlights/effect/functional.h"

namespace jazzlights {

inline FunctionalEffect solid(CRGB color, const std::string& name) {
  return effect(name,
                [color](const Frame& /*frame*/) { return [color](const Pixel& /*pt*/) -> CRGB { return color; }; });
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_SOLID_H
