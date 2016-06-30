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
  void on_render(Pixels &pixels) override {
    int32_t elapsed = get_elapsed_time();
    int width = pixels.get_width();
    int height = pixels.get_height();

    uint8_t cycleHue = speed / width * elapsed / 300;
    uint8_t riderPos = speed / width * width * elapsed / 1000;

    for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.get_coords(i, &x, &y);

      // this is the same for all values of y, so we can optimize
      uint32_t riderColor;
      {
        int brightness =
            absi(x * (256 / width) - triwave8(riderPos) * 2 + 127) * 3;
        if (brightness > 255) {
          brightness = 255;
        }
        brightness = 255 - brightness;
        riderColor = hsl(cycleHue, 255, brightness);        
      }
      pixels.set_color(i, riderColor);
    }
  }

private:
  int32_t speed;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_RIDER_H */