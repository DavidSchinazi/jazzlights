#ifndef JL_EFFECTS_MAPPING_H
#define JL_EFFECTS_MAPPING_H

#include "jazzlights/effects/functional.h"

namespace jazzlights {

inline FunctionalEffect mapping() {
  return effect("mapping", [](const Frame& frame) {
    const size_t pixelNum = (frame.pattern >> 8) & 0xFFFF;
    const bool blink = ((frame.time % 1000) < 500);
    return [pixelNum, blink](const Pixel& pt) -> CRGB {
      if (pt.index < pixelNum) {
        return CRGB::Red;
      } else if (pt.index == pixelNum) {
        if (blink) {
          return CRGB::Blue;
        } else {
          return CRGB::Black;
        }
      } else {
        return CRGB::Green;
      }
    };
  });
};

inline FunctionalEffect coloring() {
  return effect("coloring", [](const Frame& frame) {
    const uint8_t red = (frame.pattern >> 24) & 0xFF;
    const uint8_t green = (frame.pattern >> 16) & 0xFF;
    const uint8_t blue = (frame.pattern >> 8) & 0xFF;
    CRGB color(red, green, blue);
    return [color](const Pixel& /*pt*/) -> CRGB { return color; };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_MAPPING_H
