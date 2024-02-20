#ifndef JL_EFFECTS_MAPPING_H
#define JL_EFFECTS_MAPPING_H

#include "jazzlights/effects/functional.h"

namespace jazzlights {

inline FunctionalEffect mapping() {
  return effect("mapping", [](const Frame& frame) {
    const size_t pixelNum = (frame.pattern >> 8) & 0xFFFF;
    const bool blink = ((frame.time % 1000) < 500);
    return [pixelNum, blink](const Pixel& pt) -> Color {
      if (pt.index < pixelNum) {
        return CRGB::Black;
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
    const uint8_t red = ((frame.pattern >> 20) & 0x3F) << 2;
    const uint8_t green = ((frame.pattern >> 14) & 0x3F) << 2;
    const uint8_t blue = ((frame.pattern >> 8) & 0x3F) << 2;
    Color color(red, green, blue);
    return [color](const Pixel& /*pt*/) -> Color { return color; };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_MAPPING_H
