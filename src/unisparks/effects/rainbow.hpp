#ifndef UNISPARKS_EFFECTS_RAINBOW_H
#define UNISPARKS_EFFECTS_RAINBOW_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto rainbow = []() {
  return effect([](const Frame & frame) {
    Milliseconds period = adjustDuration(frame, 8000);
    int width = frame.size.width;
    int height = frame.size.height;

    uint8_t initial_hue = 255 - 255 * frame.time / period;
    double maxd = sqrt(width * width + height * height) / 2;
    int cx = width / 2; // + pulse(frame, -width/8, width/8);
    int cy = height / 2; // + pulse(frame, -height/8, height/8);

    return [ = ](Point p) -> Color {
      int dx = p.x - cx;
      int dy = p.y - cy;
      double d = sqrt(dx * dx + dy * dy);
      uint8_t hue =
      (initial_hue + int32_t(255 * d / maxd)) % 255;

      return HslColor(hue, 240, 255);
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_RAINBOW_H */

