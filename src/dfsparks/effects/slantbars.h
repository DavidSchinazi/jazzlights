#ifndef DFSPARKS_EFFECT_SLANTBARS_H
#define DFSPARKS_EFFECT_SLANTBARS_H
#include "dfsparks/effect.h"
#include "dfsparks/math.h"
#include <stdio.h>

namespace dfsparks {

class Slantbars : public Effect {
  void doRender(Pixels &pixels) final {
    int32_t elapsed = timeElapsed();
    uint8_t cycleHue = 256 * elapsed / hue_period;
    uint8_t slantPos = 256 * elapsed / mtn_period;
    int width = pixels.width();
    int height = pixels.height();

    for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.coords(i, &x, &y);
      double xx = width < 8 ? x : 8.0 * x / width;
      double yy = height < 8 ? y : 8.0 * y / height;
      pixels.setColor(
          i, hsl(cycleHue, 255, quadwave8(xx * 32 + yy * 32 + slantPos)));
    }
  }

  int32_t hue_period = 8000;
  int32_t mtn_period = 1000;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_SLANTBARS_H */
