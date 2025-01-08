#ifndef JL_EFFECT_FAIRY_WAND_H
#define JL_EFFECT_FAIRY_WAND_H

#include "jazzlights/effect/functional.h"

namespace jazzlights {

inline FunctionalEffect fairy_wand() {
  return effect("fairy-wand", [](const Frame& frame) {
    bool blink;
    if (frame.time < 1000) {
      blink = ((frame.time % 500) < 250);
    } else if (frame.time < 2000) {
      blink = ((frame.time % 250) < 125);
    } else if (frame.time < 3000) {
      blink = ((frame.time % 125) < 63);
    } else if (frame.time < 4000) {
      blink = ((frame.time % 63) < 32);
    } else if (frame.time < 7000) {
      blink = true;
    } else {
      blink = false;
    }
    return [blink](const Pixel& /*pt*/) -> CRGB {
      constexpr int32_t white = 0xffffff, black = 0;
      return CRGB(blink ? white : black);
    };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECT_FAIRY_WAND_H
