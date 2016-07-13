#ifndef DFSPARKS_EFFECT_PLASMA_H
#define DFSPARKS_EFFECT_PLASMA_H
#include "dfsparks/color.h"
#include "dfsparks/effect.h"
#include "dfsparks/math.h"

namespace dfsparks {

class Plasma : public Effect {
  void doRender(Pixels &pixels) final {
    int32_t elapsed = timeElapsed();
    int width = pixels.width();
    int height = pixels.height();

    uint8_t offset = speed * elapsed / 255;
    int plasVector = offset * 16;

    // Calculate current center of plasma pattern (can be offscreen)
    int xOffset = cos8(plasVector / 256);
    int yOffset = sin8(plasVector / 256);

    for (int i = 0; i < pixels.count(); ++i) {
      int x, y;
      pixels.coords(i, &x, &y);

      double xx = 16.0 * x / width;
      double yy = 16.0 * y / height;
      uint8_t hue = sin8(sqrt(square((xx - 7.5) * 10 + xOffset - 127) +
                              square((yy - 2) * 10 + yOffset - 127)) +
                         offset);

      pixels.setColor(i, hsl(hue, 255, 255));
    }
  }

  int32_t speed = 30;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_PLASMA_H */
