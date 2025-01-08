#ifndef JL_EFFECT_CALIBRATION_H
#define JL_EFFECT_CALIBRATION_H

#include "jazzlights/effect/functional.h"

namespace jazzlights {

inline FunctionalEffect calibration() {
  return effect("calibration", [](const Frame& frame) {
    const bool blink = ((frame.time % 1000) < 500);
    return [&frame, blink](const Pixel& pt) -> CRGB {
      XYIndex xyIndex = frame.xyIndexStore->FromPixel(pt);
      const int32_t green = 0x00ff00, blue = 0x0000ff, red = 0xff0000;
      const int32_t yellow = green | red, purple = red | blue, white = 0xffffff;
      const int32_t orange = 0xffcc00;
      constexpr int32_t yColors[19] = {red,    green, blue,   yellow, purple, orange, white, blue, yellow, red,
                                       purple, green, orange, white,  yellow, purple, green, blue, red};
      constexpr int numYColors = sizeof(yColors) / sizeof(yColors[0]);

      const size_t y = xyIndex.yIndex;
      int32_t col = yColors[y % numYColors];
#if JL_IS_CONFIG(VEST)
      const size_t x = xyIndex.xIndex;
      if (blink) {
        if (y == 0 && (x == 0 || x == 11 || x == 26)) {
          col = 0;
        } else if (y == 1 && (x == 10 || x == 25 || x == 36)) {
          col = 0;
        }
      }
#else   // VEST
    (void)blink;
#endif  // VEST
      return CRGB(col);
    };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECT_CALIBRATION_H
