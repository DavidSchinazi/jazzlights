#ifndef DFSPARKS_EFFECT_RIDER_H
#define DFSPARKS_EFFECT_RIDER_H
#include <DFSParks_Math.h>
#include <DFSparks_Effect.h>
#include <stdio.h>

namespace dfsparks {

class Rider : public Effect {
public:
  Rider(int32_t sp) : speed(sp) {}

private:
  void on_render(Matrix &pixels) override {
    int32_t elapsed = get_elapsed_time();
    int width = pixels.get_width();
    int height = pixels.get_height();

    uint8_t cycleHue = speed * elapsed / 300;
    uint8_t riderPos = speed * width * elapsed / 1000;

    for (uint8_t x = 0; x < width; x++) {
      int brightness =
          absi(x * (256 / width) - triwave8(riderPos) * 2 + 127) * 3;
      if (brightness > 255) {
        brightness = 255;
      }
      brightness = 255 - brightness;
      uint32_t riderColor = hsl(cycleHue, 255, brightness);
      for (int y = height; y >= 0; --y) {
        pixels.set_color(x, y, riderColor);
      }
    }
  }

private:
  int32_t speed;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_RIDER_H */