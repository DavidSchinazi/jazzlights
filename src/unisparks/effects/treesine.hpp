#ifndef UNISPARKS_EFFECTS_TREESINE_H
#define UNISPARKS_EFFECTS_TREESINE_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto treesine = []() {
  return effect([ = ](const Frame & frame) {
    Milliseconds period = adjustDuration(frame, 8000);
    int width = frame.size.width;
    int height = frame.size.height;

    uint8_t sineOffset = 256 * frame.time / period;

    return [ = ](Point p) -> Color {
      using namespace internal;

      // Calculate "sine" waves with varying periods
      // sin8 is used for speed; cos8, quadwave8, or triwave8 would also work
      // here
      double xx = (width > 64 ? 8.0 : 4.0) * p.x / width;
      uint8_t sinDistanceR =
      qmul8(abs(p.y * (255 / height) - sin8(sineOffset * 9 + xx * 16)), 2);
      uint8_t sinDistanceG =
      qmul8(abs(p.y * (255 / height) - sin8(sineOffset * 10 + xx * 16)), 2);
      uint8_t sinDistanceB =
      qmul8(abs(p.y * (255 / height) - sin8(sineOffset * 11 + xx * 16)), 2);

      return RgbaColor(255 - sinDistanceR, 255 - sinDistanceG, 255 - sinDistanceB);
    };
  });
};


} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_TREESINE_H */
