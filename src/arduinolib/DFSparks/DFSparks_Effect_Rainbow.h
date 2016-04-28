#ifndef DFSPARKS_EFFECT_RAINBOW_H
#define DFSPARKS_EFFECT_RAINBOW_H
#include <DFSparks_Effect.h>
#include <math.h>
#include <stdio.h>

namespace dfsparks {

template <typename Pixels> class Rainbow : public Effect<Pixels> {
public:
  Rainbow(int32_t pd) : period(pd) {}

private:
  void on_render(Pixels &pixels) override {
    int32_t elapsed = this->get_elapsed_time();
    int width = Pixels::width;
    int height = Pixels::height;

    uint8_t initial_hue = 255 - 255 * elapsed / period;
    double maxd = sqrt(width * width + height * height) / 2;
    int cx = width / 2;
    int cy = height / 2;
    for (int x = width - 1; x >= 0; --x) {
      for (int y = height - 1; y >= 0; --y) {
        int dx = x - cx;
        int dy = y - cy;
        double d = sqrt(dx * dx + dy * dy);
        uint8_t hue = (initial_hue + int32_t(255 * d / maxd)) % 255;
        uint32_t color = hsl(hue, 240, 255);
        pixels.set_color(x, y, color);
      }
    }
  }

private:
  int32_t period;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_RAINBOW_H */