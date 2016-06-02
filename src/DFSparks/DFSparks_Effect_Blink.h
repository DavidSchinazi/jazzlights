#ifndef DFSPARKS_EFFECT_BLINK_H
#define DFSPARKS_EFFECT_BLINK_H
#include <DFSparks_Effect.h>

namespace dfsparks {

class Blink : public Effect {
public:
  Blink(int32_t pd, uint32_t c1, uint32_t c2)
      : period(pd), color1(c1), color2(c2) {}

private:
  void on_start_frame() override {
  	curr_color = get_elapsed_time() / period % 2 ? color1 : color2;
  }
  void on_get_pixel_color(int x, int y) const override {
  	return curr_color;
  }

private:
  int32_t period;
  uint32_t color1;
  uint32_t color2;

  uint32_t curr_color;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_BLINK_H */