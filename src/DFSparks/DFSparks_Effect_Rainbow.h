#ifndef DFSPARKS_EFFECT_RAINBOW_H
#define DFSPARKS_EFFECT_RAINBOW_H
#include <DFSparks_Effect.h>
#include <math.h>
#include <stdio.h>

namespace dfsparks {

class Rainbow : public Effect {
public:
  Rainbow(int32_t pd) : period(pd) {}

private:
  void on_render(Pixels &pixels) override {
    int32_t elapsed = get_elapsed_time();
    int width = pixels.get_width();
    int height = pixels.get_height();

    uint8_t initial_hue = 255 - 255 * elapsed / period;
    double maxd = sqrt(width * width + height * height) / 2;
    int cx = width / 2;
    int cy = height / 2;
  
    for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.get_coords(i, &x, &y);

      int dx = x - cx;
      int dy = y - cy;
      double d = sqrt(dx * dx + dy * dy);
      uint8_t hue = (initial_hue + int32_t(255 * d / maxd)) % 255;
      uint32_t color = hsl(hue, 240, 255);
      pixels.set_color(i, color);    
    }
  }

private:
  int32_t period;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_RAINBOW_H */