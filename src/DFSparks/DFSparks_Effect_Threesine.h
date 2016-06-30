#ifndef DFSPARKS_EFFECT_THREESINE_H
#define DFSPARKS_EFFECT_THREESINE_H
#include <DFSParks_Math.h>
#include <DFSparks_Effect.h>

namespace dfsparks {

class Threesine : public Effect {
public:
  Threesine(int32_t sp) : speed(sp) {}

private:
  void on_render(Pixels &pixels) override {
    int32_t elapsed = get_elapsed_time();
    int width = pixels.get_width();
    int height = pixels.get_height();

    uint8_t sineOffset = speed * elapsed / 255;

   for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.get_coords(i, &x, &y);

      // Calculate "sine" waves with varying periods
      // sin8 is used for speed; cos8, quadwave8, or triwave8 would also work
      // here
      uint8_t sinDistanceR =
          qmul8(abs(y * (255 / height) - sin8(sineOffset * 9 + x * 16)), 2);
      uint8_t sinDistanceG =
          qmul8(abs(y * (255 / height) - sin8(sineOffset * 10 + x * 16)), 2);
      uint8_t sinDistanceB =
          qmul8(abs(y * (255 / height) - sin8(sineOffset * 11 + x * 16)), 2);

      pixels.set_color(i, rgb(255 - sinDistanceR, 255 - sinDistanceG,
                                 255 - sinDistanceB));
    }
    sineOffset++; // byte will wrap from 255 to 0, matching sin8 0-255 cycle
  }

private:
  int32_t speed;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_THREESINE_H */