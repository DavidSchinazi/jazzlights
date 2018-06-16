#ifndef UNISPARKS_EFFECTS_GLITTER_H
#define UNISPARKS_EFFECTS_GLITTER_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto glitter = []() {
  return effect([ = ](const Frame & frame) {
    uint8_t hue = cycleHue(frame);

    return [ = ](Point /*pt*/) -> Color {
      using internal::random8;
      return HslColor(hue, 255, random8(5) * 63);
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_GLITTER_H */
