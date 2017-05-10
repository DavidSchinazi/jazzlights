#include "dfsparks/effects/flame.h"

#include <assert.h>
#include "dfsparks/color.h"
#include "dfsparks/math.h"

namespace dfsparks {

inline uint8_t random_(uint8_t from = 0, uint8_t to = 255) {
  return from + rand() % (to - from);
}

RgbaColor heatColor(uint8_t temperature) {
  uint8_t r, g, b;
  // return rgb(temperature,temperature,temperature);

  // Scale 'heat' down from 0-255 to 0-191,
  // which can then be easily divided into three
  // equal 'thirds' of 64 units each.
  uint8_t t192 = scale8_video(temperature, 192);

  // calculate a value that ramps up from
  // zero to 255 in each 'third' of the scale.
  uint8_t heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2;                 // scale up to 0..252

  // now figure out which third of the spectrum we're in:
  if (t192 & 0x80) {
    // we're in the hottest third
    r = 255;      // full red
    g = 255;      // full green
    b = heatramp; // ramp up blue

  } else if (t192 & 0x40) {
    // we're in the middle third
    r = 255;      // full red
    g = heatramp; // ramp up green
    b = 0;        // no blue

  } else {
    // we're in the coolest third
    r = heatramp; // ramp up red
    g = 0;        // no green
    b = 0;        // no blue
  }

  return RgbaColor(r, g, b);
}

void renderFlame(Pixels& pixels, const Frame& frame, uint8_t* heat) {
  constexpr int cooling = 120;

  int width = pixels.width();
  int height = pixels.height();
  int freq = 50;
  int step = 0;

  if (frame.timeElapsed / freq != step) {
    step = frame.timeElapsed / freq;
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
             heat[(k - 2) * width + x]) /
            3;
      }

      // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
      // if (random8() < sparkling) {
      //   int y = random8(min(height, 2));
      //   heat[y * width + x] = qadd8(heat[y * width + x], random8(160,
      //   255));
      // }
      heat[x] = random_(160, 255);
    }
  }

  // Step 4.  Map from heat cells to LED colors
  for (int i = 0; i < pixels.count(); ++i) {
    Point pt = pixels.coords(i);
    pixels.setColor(i, heatColor(heat[(height - pt.y - 1) * width + pt.x]));
  }
}

} // namespace dfsparks
