#ifndef UNISPARKS_EFFECTS_SLANTBARS_H
#define UNISPARKS_EFFECTS_SLANTBARS_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto slantbars = []() {
  return effect([](const Frame & frame) {
    int32_t mtn_period = adjustDuration(frame, 1000);
    uint8_t slantPos = 256 * frame.time / mtn_period;
    int w = width(frame);
    int h = height(frame);

    return [ = ](const Pixel& px) -> Color {
      using namespace internal;

      double xx = w < 8 ? px.coord.x : 8.0 * px.coord.x / w;
      double yy = w < 8 ? px.coord.y : 8.0 * px.coord.y / h;
      return HslColor(cycleHue(frame), 255, quadwave8(xx * 32 + yy * 32 + slantPos));
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_SLANTBARS_H */

