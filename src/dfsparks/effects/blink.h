#ifndef DFSPARKS_EFFECT_BLINK_H
#define DFSPARKS_EFFECT_BLINK_H
#include "dfsparks/effect.h"

namespace dfsparks {

class Blink : public Effect {
  void doRender(Pixels &pixels) final {
    int32_t elapsed = timeElapsed();
    pixels.fill(elapsed / period % 2 ? color1 : color2);
  }

  int32_t period = 1000;
  uint32_t color1 = 0xff0000;
  uint32_t color2 = 0x00ff00;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_BLINK_H */
