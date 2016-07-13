#ifndef DFSPARKS_EFFECT_THREESINE_H
#define DFSPARKS_EFFECT_THREESINE_H
#include "dfsparks/effect.h"
#include "dfsparks/math.h"

namespace dfsparks {

class Threesine : public Effect {
  void doRender(Pixels &pixels) final {
    int32_t elapsed = timeElapsed();
    int width = pixels.width();
    int height = pixels.height();

    // uint8_t sineOffset = speed * elapsed / 255;
    uint8_t sineOffset = 256 * elapsed / period;

    for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.coords(i, &x, &y);

      // Calculate "sine" waves with varying periods
      // sin8 is used for speed; cos8, quadwave8, or triwave8 would also work
      // here
      double xx = (width > 64 ? 8.0 : 4.0) * x / width;
      uint8_t sinDistanceR =
          qmul8(abs(y * (255 / height) - sin8(sineOffset * 9 + xx * 16)), 2);
      uint8_t sinDistanceG =
          qmul8(abs(y * (255 / height) - sin8(sineOffset * 10 + xx * 16)), 2);
      uint8_t sinDistanceB =
          qmul8(abs(y * (255 / height) - sin8(sineOffset * 11 + xx * 16)), 2);
      pixels.setColor(
          i, rgb(255 - sinDistanceR, 255 - sinDistanceG, 255 - sinDistanceB));
    }
    sineOffset++; // byte will wrap from 255 to 0, matching sin8 0-255 cycle
  }

  int32_t period = 8000;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_THREESINE_H */
