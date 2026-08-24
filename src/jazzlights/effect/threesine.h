#ifndef JL_EFFECT_THREESINE_H
#define JL_EFFECT_THREESINE_H
#include "jazzlights/effect/functional.h"
#include "jazzlights/render/fastled_wrapper.h"

namespace jazzlights {

inline FunctionalEffect Threesine() {
  return FunctionalEffectFrom("threesine", [](const Frame& frame) {
    const Coord w = Width(frame);
    const Coord h = Height(frame);
    const uint8_t sineOffset = 256 * frame.time / kEffectDurationMs;
    return [w, h, sineOffset](const Pixel& px) -> CRGB {
      // Calculate "sine" waves with varying periods
      // sin8 is used for speed; cos8, quadwave8, or triwave8 would also work
      // here
      double xx = (w > 64 ? 8.0 : 4.0) * px.coord.x / w;
      uint8_t sinDistanceR = qmul8(abs(px.coord.y * (255 / h) - sin8(sineOffset * 9 + xx * 16)), 2);
      uint8_t sinDistanceG = qmul8(abs(px.coord.y * (255 / h) - sin8(sineOffset * 10 + xx * 16)), 2);
      uint8_t sinDistanceB = qmul8(abs(px.coord.y * (255 / h) - sin8(sineOffset * 11 + xx * 16)), 2);

      return CRGB(255 - sinDistanceR, 255 - sinDistanceG, 255 - sinDistanceB);
    };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECT_THREESINE_H
