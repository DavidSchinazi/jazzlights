#ifndef JL_EFFECTS_FOLLOWSTRAND_H
#define JL_EFFECTS_FOLLOWSTRAND_H

#include "jazzlights/effects/functional.h"
#include "jazzlights/layout/layout.h"

namespace jazzlights {

inline FunctionalEffect follow_strand() {
  return effect("follow-strand", [](const Frame& frame) {
    const size_t offset = frame.time / 100;
    const bool blink = ((frame.time % 1000) < 500);
    return [offset, blink](const Pixel& pt) -> CRGB {
      constexpr int32_t green = 0x00ff00, blue = 0x0000ff, red = 0xff0000, black = 0;
      constexpr int32_t colors[] = {
          red,   red,   red,   black, black, black, black, black, black, green, green, green, black, black,
          black, black, black, black, blue,  blue,  blue,  black, black, black, black, black, black,
      };
      constexpr size_t numColors = sizeof(colors) / sizeof(colors[0]);
      const size_t reverseIndex = (-pt.index % numColors) + numColors - 1;
      int32_t col = colors[(offset + reverseIndex) % numColors];
      if (pt.index == 0 ||
          (fabs(pt.coord.x - pt.layout->at(0).x) < 0.001 && fabs(pt.coord.y - pt.layout->at(0).y) < 0.001)) {
        col = blink ? 0xffffff : 0;
      } else if (pt.index == pt.layout->pixelCount() - 1 ||
                 (fabs(pt.coord.x - pt.layout->at(pt.layout->pixelCount() - 1).x) < 0.001 &&
                  fabs(pt.coord.y - pt.layout->at(pt.layout->pixelCount() - 1).y) < 0.001)) {
        col = blink ? 0xff00ff : 0;
      }
      return CRGB(col);
    };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_FOLLOWSTRAND_H
