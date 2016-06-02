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
  void on_start_frame() override {
    int32_t elapsed = get_elapsed_time();
    int x0, y0, width, height; 
    get_frame_bounds(x0, y0, width, height);

    uint8_t initial_hue = 255 - 255 * elapsed / period;
    double maxd = sqrt(width * width + height * height) / 2;
    int cx = x0 + width / 2;
    int cy = y0 + height / 2;
  }

  void on_get_pixel_color(int x, int y) const override {
      int dx = x - cx;
      int dy = y - cy;
      double d = sqrt(dx * dx + dy * dy);
      uint8_t hue = (initial_hue + int32_t(255 * d / maxd)) % 255;
      uint32_t color = hsl(hue, 240, 255);
      pixels.set_color(x, y, color);
  }

private:
  int32_t period;

  uint8_t initial_hue;
  int cx, xy;
  double maxd;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_RAINBOW_H */