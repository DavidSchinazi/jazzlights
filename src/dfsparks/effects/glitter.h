#ifndef DFSPARKS_EFFECT_GLITTER_H
#define DFSPARKS_EFFECT_GLITTER_H
#include "dfsparks/effect.h"
#include "dfsparks/math.h"
#include <stdio.h>

namespace dfsparks {

class Glitter : public Effect {
  void on_render(Pixels &pixels) final {
    int32_t elapsed = get_elapsed_time();
    uint8_t cycleHue = 256 * elapsed / hue_period;

    for (int i = 0; i < pixels.count(); ++i) {
      pixels.set_color(i, hsl(cycleHue, 255, random8(5) * 63));
    }
  }

  int32_t hue_period = 8000;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_GLITTER_H */
