#include "unisparks/effects/flame.hpp"

#include <assert.h>

#include "unisparks/player.hpp"
#include "unisparks/util/color.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {
using internal::scale8_video;
using internal::qsub8;

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

struct Context {
  Milliseconds t;
  uint8_t heat[];
};

size_t Flame::contextSize(const Frame& frame) const {
  const int w = frame.xyIndexStore->xValuesCount();
  const int h = frame.xyIndexStore->yValuesCount();
  return sizeof(Context) + sizeof(uint8_t) * w * h;
}

void Flame::begin(const Frame& frame) const {
  const int w = frame.xyIndexStore->xValuesCount();
  const int h = frame.xyIndexStore->yValuesCount();
  Context& ctx = *static_cast<Context*>(frame.context);
  memset(ctx.heat, 0, sizeof(uint8_t) * w * h);
}

void Flame::rewind(const Frame& frame) const {
  const int w = frame.xyIndexStore->xValuesCount();
  const int h = frame.xyIndexStore->yValuesCount();
  Context& ctx = *static_cast<Context*>(frame.context);

  for (int x = 0; x < w; x++) {
    // Step 1.  Cool down every cell a little
    for (int y = 0; y < h; y++) {
      ctx.heat[y * w + x] = qsub8(ctx.heat[y * w + x],
                                  frame.predictableRandom->GetRandomNumberBetween(0, 1200 / h + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int y2 = h - 1; y2 >= 3; y2--) {
      ctx.heat[y2 * w + x] =
        (static_cast<uint16_t>(ctx.heat[(y2 - 1) * w + x]) +
         static_cast<uint16_t>(ctx.heat[(y2 - 2) * w + x]) +
         static_cast<uint16_t>(ctx.heat[(y2 - 3) * w + x])
        ) / 3;
    }
    ctx.heat[2 * w + x] =
      (static_cast<uint16_t>(ctx.heat[1 * w + x]) +
       static_cast<uint16_t>(ctx.heat[0 * w + x])
      ) / 2;
    ctx.heat[1 * w + x] = ctx.heat[0 * w + x];

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    ctx.heat[x] = frame.predictableRandom->GetRandomNumberBetween(160, 255);
  }
}

Color Flame::color(const Frame& frame, const Pixel& px) const {
  // TODO understand why this patern lights up the top row.
  const int w = frame.xyIndexStore->xValuesCount();
  const int h = frame.xyIndexStore->yValuesCount();
  Context& ctx = *static_cast<Context*>(frame.context);
  XYIndex xyIndex = frame.xyIndexStore->FromPixel(px);
  return Color(heatColor(ctx.heat[(h - xyIndex.yIndex) * w + xyIndex.xIndex])).lightnessToAlpha();
}



} // namespace unisparks
