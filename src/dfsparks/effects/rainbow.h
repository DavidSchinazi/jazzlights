#ifndef DFSPARKS_EFFECT_RAINBOW_H
#define DFSPARKS_EFFECT_RAINBOW_H
#include "dfsparks/effect.h"
#include <math.h>
#include <stdio.h>

namespace dfsparks {

class Rainbow : public Effect {
  void doRender(Pixels &pixels) final {
    int32_t elapsed = timeElapsed();
    int width = pixels.width();
    int height = pixels.height();

    uint8_t initial_hue = 255 - 255 * elapsed / period;
    double maxd = sqrt(width * width + height * height) / 2;
    int cx = width / 2;
    int cy = height / 2;

    for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.coords(i, &x, &y);

      int dx = x - cx;
      int dy = y - cy;
      double d = sqrt(dx * dx + dy * dy);
      uint8_t hue =
          (initial_hue + int32_t(255 * d / maxd)) % 255; 
      uint32_t color = hsl(hue, 240, 255);
      pixels.setColor(i, color);
    }
  }

  int32_t period = 1800;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_RAINBOW_H */
