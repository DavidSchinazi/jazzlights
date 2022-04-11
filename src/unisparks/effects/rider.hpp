#ifndef UNISPARKS_EFFECTS_RIDER_H
#define UNISPARKS_EFFECTS_RIDER_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {


auto rider = []() {
  return effect("rider", [](const Frame & frame) {
    Milliseconds riderPeriod = adjustDuration(frame, 2000);
    Milliseconds huePeriod = adjustDuration(frame, 8000);

    auto sz = size8(frame);
    uint8_t cycleHue = 256 * frame.time / huePeriod;
    uint8_t riderPos = 256 * frame.time / riderPeriod;

    return [ = ](const Pixel & px) -> Color {
      using namespace internal;
      auto p = pos8(px);

      // this is the same for all values of y, so we can potentially optimize
      HslColor riderColor;
      int brightness =
      absi(p.x * (256 / (sz.width % 256)) - triwave8(riderPos) * 2 + 127) * 3;
      if (brightness > 255) {
        brightness = 255;
      }
      brightness = 255 - brightness;
      riderColor = HslColor(cycleHue, 255, brightness);
      return riderColor;
    };
  });
};

auto rider2 = []() {
  return effect("rider2", [](const Frame & frame) {
    Milliseconds riderPeriod = adjustDuration(frame, 2000);
    uint8_t hue = cycleHue(frame);
    Coord riderWidth = width(frame) / 4;
    Coord riderPos = 0.5 * width(frame) + 0.5 * (2 * riderWidth + width(
                       frame)) * triwave(1.0 * frame.time / riderPeriod);

    return [ = ](const Pixel & p) -> Color {
      // this is the same for all values of y, so we can potentially optimize
      double d = fabs(p.coord.x - riderPos);
      uint8_t brightness =  d > riderWidth ? 0 : uint8_t(255 * (1 - triwave(d / 2)));
      return HslColor(hue, 255, brightness);
    };
  });
};


} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_RIDER_H */

