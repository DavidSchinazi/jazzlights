#ifndef DFSPARKS_EFFECT_PLASMA_H
#define DFSPARKS_EFFECT_PLASMA_H
#include <DFSParks_Color.h>
#include <DFSParks_Math.h>
#include <DFSparks_Effect.h>

namespace dfsparks {

class Plasma : public Effect {
public:
  Plasma(int32_t sp) : speed(sp) {}

private:
  void on_start_frame() override {
    int32_t elapsed = get_elapsed_time();
    int x0, y0, width, height; 
    get_frame_bounds(x0, y0, width, height);

    uint8_t offset = speed * elapsed / 255;
    int plasVector = offset * 16;

    // Calculate current center of plasma pattern (can be offscreen)
    xOffset = cos8(plasVector / 256);
    yOffset = sin8(plasVector / 256);
  }

  void on_get_pixel_color(int x, int y) const override {
    uint8_t hue = sin8(sqrt(square(((float)x - 7.5) * 10 + xOffset - 127) +
                            square(((float)y - 2) * 10 + yOffset - 127)) +
                       offset);
    return hsl(hue, 255, 255);
  }

  int32_t speed;

  int xOffset;
  int yOffset;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_PLASMA_H */