#ifndef DFSPARKS_EFFECT_SLANTBARS_H
#define DFSPARKS_EFFECT_SLANTBARS_H
#include "dfsparks/effect.h"
#include "dfsparks/math.h"
#include <stdio.h>

namespace dfsparks {

class Slantbars : public Effect {
  void on_render(Pixels &pixels) final {
    int32_t elapsed = get_elapsed_time();
    uint8_t cycleHue = 256 * elapsed / hue_period;
    uint8_t slantPos = 256 * elapsed / mtn_period;
    int width = pixels.get_width();
    int height = pixels.get_height();

    for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.get_coords(i, &x, &y);
      double xx = width < 8 ? x : 8.0 * x / width;
      double yy = height < 8 ? y : 8.0 * y / height;
      pixels.set_color(
          i, hsl(cycleHue, 255, quadwave8(xx * 32 + yy * 32 + slantPos)));
    }
  }

  int32_t hue_period = 8000;
  int32_t mtn_period = 1000;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_SLANTBARS_H */
