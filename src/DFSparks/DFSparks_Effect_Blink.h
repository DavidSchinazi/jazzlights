#ifndef DFSPARKS_EFFECT_BLINK_H
#define DFSPARKS_EFFECT_BLINK_H
#include <DFSparks_Effect.h>

namespace dfsparks {

class Blink : public Effect {
public:
  Blink(int32_t pd, uint32_t c1, uint32_t c2)
      : period(pd), color1(c1), color2(c2) {}

private:
  void on_render(Pixels &pixels) override {
    int32_t elapsed = get_elapsed_time();
    pixels.fill(elapsed / period % 2 ? color1 : color2);
  }

private:
  int32_t period;
  uint32_t color1;
  uint32_t color2;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_BLINK_H */