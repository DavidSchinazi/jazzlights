#ifndef DFSPARKS_EFFECT_GLITTER_H
#define DFSPARKS_EFFECT_GLITTER_H
#include "dfsparks/effect.h"
#include "dfsparks/math.h"
#include <stdio.h>

namespace dfsparks {

class Glitter : public Effect {
  void doRender(Pixels &pixels) final {
    int32_t elapsed = timeElapsed();
    uint8_t cycleHue = 256 * elapsed / hue_period;

    for (int i = 0; i < pixels.count(); ++i) {
      pixels.setColor(i, hsl(cycleHue, 255, random8(5) * 63));
    }
  }

  int32_t hue_period = 8000;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_GLITTER_H */
