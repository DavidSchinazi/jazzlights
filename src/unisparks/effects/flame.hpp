#ifndef UNISPARKS_EFFECT_FLAME_H
#define UNISPARKS_EFFECT_FLAME_H
#include <stdlib.h>
#include "unisparks/effect.hpp"
namespace unisparks {

#if 0
void flameColor(Frame fr, Point pt);

constexpr auto flame = []() {
  return effect([ = ](const Frame & frame) {
    return [ = ](Point pt) -> Color {
      return flameColor(fr, pt);
    };
  });
};
#endif


} // namespace unisparks
#endif /* UNISPARKS_EFFECT_FLAME_H */
