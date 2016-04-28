#ifndef DFSPARKS_EFFECT_RIDER_H
#define DFSPARKS_EFFECT_RIDER_H
#include <DFSParks_Math.h>
#include <DFSparks_Effect.h>
#include <stdio.h>

namespace dfsparks {

template <typename Pixels> class Rider : public Effect<Pixels> {
public:
  Rider(int32_t sp) : speed(sp) {}

private:
  void on_render(Pixels &pixels) override {
    int32_t elapsed = this->get_elapsed_time();
    int width = Pixels::width;
    int height = Pixels::height;

    uint8_t cycleHue = 0;
    static uint8_t riderPos = 0; // = (elapsed / 100) % 255;

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
    riderPos++;
  }

private:
  int32_t speed;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_RIDER_H */