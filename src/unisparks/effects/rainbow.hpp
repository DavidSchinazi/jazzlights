#ifndef UNISPARKS_EFFECTS_RAINBOW_H
#define UNISPARKS_EFFECTS_RAINBOW_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto rainbow = []() {
  return effect([](const Frame & frame) {
    Milliseconds period = adjustDuration(frame, 8000);
    uint8_t initial_hue = 255 - 255 * frame.time / period;
    double maxd = distance(center(frame), lefttop(frame));

    return [ = ](const Pixel& px) -> Color {
      double d = distance(px.coord, center(frame));
      uint8_t hue = (initial_hue + int32_t(255 * d / maxd)) % 255;
      return HslColor(hue, 240, 255);
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_RAINBOW_H */

