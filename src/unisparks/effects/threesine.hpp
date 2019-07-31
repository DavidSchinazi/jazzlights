#ifndef UNISPARKS_EFFECTS_THREESINE_H
#define UNISPARKS_EFFECTS_THREESINE_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto threesine = []() {
  return effect([ = ](const Frame & frame) {
    Milliseconds period = adjustDuration(frame, 8000);
    Coord w = width(frame);
    Coord h = height(frame);

    uint8_t sineOffset = 256 * frame.time / period;

    return [ = ](const Pixel& px) -> Color {
      using namespace internal;

      // Calculate "sine" waves with varying periods
      // sin8 is used for speed; cos8, quadwave8, or triwave8 would also work
      // here
      double xx = (w > 64 ? 8.0 : 4.0) * px.coord.x / w;
      uint8_t sinDistanceR =
      qmul8(abs(px.coord.y * (255 / h) - sin8(sineOffset * 9 + xx * 16)), 2);
      uint8_t sinDistanceG =
      qmul8(abs(px.coord.y * (255 / h) - sin8(sineOffset * 10 + xx * 16)), 2);
      uint8_t sinDistanceB =
      qmul8(abs(px.coord.y * (255 / h) - sin8(sineOffset * 11 + xx * 16)), 2);

      return RgbaColor(255 - sinDistanceR, 255 - sinDistanceG, 255 - sinDistanceB);
    };
  });
};


} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_THREESINE_H */
