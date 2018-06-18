#ifndef UNISPARKS_EFFECTS_RIDER_H
#define UNISPARKS_EFFECTS_RIDER_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto rider = []() {
  return effect([](const Frame & frame) {
    Milliseconds riderPeriod = adjustDuration(frame, 2000);
    Milliseconds huePeriod = adjustDuration(frame, 8000);
    uint8_t cycleHue = 256 * frame.time / huePeriod;
    uint8_t riderPos = 256 * frame.time / riderPeriod;

    return [ = ](const Pixel& p) -> Color {
      using namespace internal;

      // this is the same for all values of y, so we can potentially optimize
      int brightness = absi(p.coord.x * (256 / (int(width(frame)) % 256)) 
        - triwave8(riderPos) * 2 + 127) * 3;
      if (brightness > 255) {
      brightness = 255;
    }
    brightness = 255 - brightness;
    return HslColor(cycleHue, 255, brightness);
  };
});
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_RIDER_H */

