#ifndef UNISPARKS_EFFECTS_GLITTER_H
#define UNISPARKS_EFFECTS_GLITTER_H

#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

auto glitter = []() {
  return effect("glitter", [ = ](const Frame & frame) {
    const uint8_t hue = cycleHue(frame);
    PredictableRandom& predictableRandom = frame.player->predictableRandom();
    return [hue, &predictableRandom](const Pixel& /*pt*/) -> Color {
      return HslColor(hue, 255, predictableRandom.GetRandomNumberBetween(0, 5) * 63);
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_GLITTER_H */
