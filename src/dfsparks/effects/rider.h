#ifndef DFSPARKS_EFFECT_RIDER_H
#define DFSPARKS_EFFECT_RIDER_H
#include "dfsparks/effect.h"
#include "dfsparks/math.h"
#include <stdio.h>

namespace dfsparks {

class Rider : public Effect {
  void doRender(Pixels &pixels) final {
    int32_t elapsed = timeElapsed();
    int width = pixels.width();

    uint8_t cycleHue = 256 * elapsed / hue_period;
    uint8_t riderPos = 256 * elapsed / rider_period;

    for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.coords(i, &x, &y);

      // this is the same for all values of y, so we can optimize
      uint32_t riderColor;
      {
        int brightness =
            absi(x * (256 / (width % 256)) - triwave8(riderPos) * 2 + 127) * 3;
        if (brightness > 255) {
          brightness = 255;
        }
        brightness = 255 - brightness;
        riderColor = hsl(cycleHue, 255, brightness);
      }
      pixels.setColor(i, riderColor);
    }
  }

  int32_t rider_period = 3000;
  int32_t hue_period = 10000;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_RIDER_H */
