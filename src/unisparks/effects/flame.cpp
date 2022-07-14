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

static constexpr int cooling = 120;

struct Context {
  Milliseconds t;
  uint8_t heat[];
};

size_t Flame::contextSize(const Frame& frame) const {
  return sizeof(Context) + frame.xValues.size() * frame.yValues.size();
}

void Flame::begin(const Frame& frame) const {
  Context& ctx = *static_cast<Context*>(frame.context);
  memset(ctx.heat, 0, frame.xValues.size() * frame.yValues.size());
}

void Flame::rewind(const Frame& frame) const {
  Context& ctx = *static_cast<Context*>(frame.context);
  const int w = frame.xValues.size();
  const int h = frame.yValues.size();

  //heat_t += freq;
  for (int x = 0; x < w; ++x) {

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < h; i++) {
      ctx.heat[i * w + x] = qsub8(
                              ctx.heat[i * w + x], frame.predictableRandom->GetRandomNumberBetween(0, ((cooling * 10) / h) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = h - 3; k > 0; k--) {
      ctx.heat[k * w + x] =
        (ctx.heat[(k - 1) * w + x] + ctx.heat[(k - 2) * w + x] +
         ctx.heat[(k - 2) * w + x]) /
        3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    // if (random8() < sparkling) {
    //   int y = random8(min(h, 2));
    //   ctx.heat[y * w + x] = qadd8(heat[y * w + x], random8(160,
    //   255));
    // }
    ctx.heat[x] = frame.predictableRandom->GetRandomNumberBetween(160, 255);
  }
}

Color Flame::color(const Frame& frame, const Pixel& px) const {
  // TODO understand why this patern lights up the top row.
  Context& ctx = *static_cast<Context*>(frame.context);
  const int w = frame.xValues.size();
  const int h = frame.yValues.size();
  int xIndex = 0;
  for (int xi = 0; xi < w; xi++) {
    if (frame.xValues[xi] == px.coord.x) {
      xIndex = xi;
      break;
    }
  }
  int yIndex = 0;
  for (int yi = 0; yi < h; yi++) {
    if (frame.yValues[yi] == px.coord.y) {
      yIndex = yi;
      break;
    }
  }
  return Color(heatColor(ctx.heat[(h - yIndex) * w + xIndex])).lightnessToAlpha();
}



} // namespace unisparks
