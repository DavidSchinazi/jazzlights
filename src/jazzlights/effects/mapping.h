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
        return RED;
      } else if (pt.index == pixelNum) {
        if (blink) {
          return BLUE;
        } else {
          return BLACK;
        }
      } else {
        return GREEN;
      }
    };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_MAPPING_H
