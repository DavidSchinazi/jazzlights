#ifndef DFSPARKS_EFFECT_FLAME_H
#define DFSPARKS_EFFECT_FLAME_H
#include <DFSParks_Effect.h>
#include <DFSParks_Math.h>
#include <stdio.h>
#include <stdlib.h>

namespace dfsparks {

uint32_t heat_color(uint8_t temperature);
inline uint8_t random_(uint8_t from=0, uint8_t to=255) {
  return from + rand() % (to - from);
}


template <typename Pixels> class Flame : public Effect<Pixels> {
private:
  void on_render(Pixels &pixels) override {
    int32_t elapsed = this->get_elapsed_time();
    int width = Pixels::width;
    int height = Pixels::height;
    int freq = 50;

    if (elapsed / freq != step) {
      step = elapsed / freq;
      for (int x = 0; x < width; ++x) {

        // Step 1.  Cool down every cell a little
        for (int i = 0; i < height; i++) {
          heat[i * width + x] = qsub8(
              heat[i * width + x], random_(0, ((cooling * 10) / height) + 2));
        }

        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (int k = height - 3; k > 0; k--) {
          heat[k * width + x] =
              (heat[(k - 1) * width + x] + heat[(k - 2) * width + x] +
               heat[(k - 2) * width + x]) / 3;
        }

        // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
        // if (random8() < sparkling) {
        //   int y = random8(min(height, 2));
        //   heat[y * width + x] = qadd8(heat[y * width + x], random8(160, 255));
        // }
        heat[x] = random_(160, 255);
      }
    }

    // for(int x = 0; x < width; ++x) {
    //   for( int y = 0; y < height; ++y) {
    //     heat[y*width+x] = 255;
    //   }
    // }
    // Step 4.  Map from heat cells to LED colors
    for (int x = 0; x < width; ++x) {
      for (int j = 0; j < height; j++) {
        pixels.set_color(x, j, heat_color(heat[j * width + x]));
      }
    }
  }

  int step = 0;
  int cooling = 120;
  uint8_t sparkling = 255;
  uint8_t heat[Pixels::width *
               Pixels::height]; // Array of temperature readings at each
                                // simulation cell
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_FLAME_H */
