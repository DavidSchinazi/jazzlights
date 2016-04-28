#include "DFSparks_Color.h"

namespace dfsparks {

// Courtecy of FastLED library, https://github.com/FastLED/FastLED
//
// The MIT License (MIT)
//
// Copyright (c) 2013 FastLED
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of
// the Software, and to permit persons to whom the Software is furnished to do
// so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
uint32_t hsl(uint8_t hue, uint8_t saturation, uint8_t value) {
  constexpr uint8_t HSV_SECTION_6 = 0x20;
  constexpr uint8_t HSV_SECTION_3 = 0x40;

  // The brightness floor is minimum number that all of
  // R, G, and B will be set to.
  uint8_t invsat = 255 - saturation;
  uint8_t brightness_floor = (value * invsat) / 256;

  // The color amplitude is the maximum amount of R, G, and B
  // that will be added on top of the brightness_floor to
  // create the specific hue desired.
  uint8_t color_amplitude = value - brightness_floor;

  // Figure out which section of the hue wheel we're in,
  // and how far offset we are withing that section
  uint8_t section = hue / HSV_SECTION_3; // 0..2
  uint8_t offset = hue % HSV_SECTION_3;  // 0..63

  uint8_t rampup = offset;                         // 0..63
  uint8_t rampdown = (HSV_SECTION_3 - 1) - offset; // 63..0

  // We now scale rampup and rampdown to a 0-255 range -- at least
  // in theory, but here's where architecture-specific decsions
  // come in to play:
  // To scale them up to 0-255, we'd want to multiply by 4.
  // But in the very next step, we multiply the ramps by other
  // values and then divide the resulting product by 256.
  // So which is faster?
  //   ((ramp * 4) * othervalue) / 256
  // or
  //   ((ramp    ) * othervalue) /  64
  // It depends on your processor architecture.
  // On 8-bit AVR, the "/ 256" is just a one-cycle register move,
  // but the "/ 64" might be a multicycle shift process. So on AVR
  // it's faster do multiply the ramp values by four, and then
  // divide by 256.
  // On ARM, the "/ 256" and "/ 64" are one cycle each, so it's
  // faster to NOT multiply the ramp values by four, and just to
  // divide the resulting product by 64 (instead of 256).
  // Moral of the story: trust your profiler, not your insticts.

  // Since there's an AVR assembly version elsewhere, we'll
  // assume what we're on an architecture where any number of
  // bit shifts has roughly the same cost, and we'll remove the
  // redundant math at the source level:

  //  // scale up to 255 range
  //  //rampup *= 4; // 0..252
  //  //rampdown *= 4; // 0..252

  // compute color-amplitude-scaled-down versions of rampup and rampdown
  uint8_t rampup_amp_adj = (rampup * color_amplitude) / (256 / 4);
  uint8_t rampdown_amp_adj = (rampdown * color_amplitude) / (256 / 4);

  // add brightness_floor offset to everything
  uint8_t rampup_adj_with_floor = rampup_amp_adj + brightness_floor;
  uint8_t rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;

  uint8_t r, g, b;
  if (section) {
    if (section == 1) {
      // section 1: 0x40..0x7F
      r = brightness_floor;
      g = rampdown_adj_with_floor;
      b = rampup_adj_with_floor;
    } else {
      // section 2; 0x80..0xBF
      r = rampup_adj_with_floor;
      g = brightness_floor;
      b = rampdown_adj_with_floor;
    }
  } else {
    // section 0: 0x00..0x3F
    r = rampdown_adj_with_floor;
    g = rampup_adj_with_floor;
    b = brightness_floor;
  }
  return rgb(r, g, b);
}

} // namespace dfsparks
