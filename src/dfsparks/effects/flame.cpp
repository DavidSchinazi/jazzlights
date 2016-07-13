#include "dfsparks/effects/flame.h"
#include "dfsparks/color.h"
#include "dfsparks/math.h"

namespace dfsparks {

uint32_t heatColor(uint8_t temperature) {
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

  return rgb(r, g, b);
}

} // namespace dfsparks
