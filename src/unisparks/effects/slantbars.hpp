#ifndef UNISPARKS_EFFECTS_SLANTBARS_H
#define UNISPARKS_EFFECTS_SLANTBARS_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto slantbars = []() {
  return effect([](const Frame & frame) {
    int32_t mtn_period = adjustDuration(frame, 1000);
    uint8_t slantPos = 256 * frame.time / mtn_period;
    int width = frame.size.width;
    int height = frame.size.height;

    return [ = ](Point p) -> Color {
      using namespace internal;

      double xx = width < 8 ? p.x : 8.0 * p.x / width;
      double yy = height < 8 ? p.y : 8.0 * p.y / height;
      return HslColor(cycleHue(frame), 255, quadwave8(xx * 32 + yy * 32 + slantPos));
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_SLANTBARS_H */

