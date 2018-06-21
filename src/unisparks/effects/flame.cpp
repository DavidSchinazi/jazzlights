#include "unisparks/effects/flame.hpp"
#include <assert.h>
#include "unisparks/util/color.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {
using internal::scale8_video;
using internal::qsub8;
using internal::random8seed;
using internal::random8;


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

static constexpr int cooling = 40;

struct Context {
  Milliseconds t;
  uint8_t heat[];
};

size_t Flame::contextSize(const Animation& a) const {
  return sizeof(Context) + a.viewport.size.width * a.viewport.size.height;
}

void Flame::begin(const Frame& frame) const {
  Context& ctx = *static_cast<Context*>(frame.animation.context);
  memset(ctx.heat, 0, width(frame)*height(frame));
}

void Flame::rewind(const Frame& frame) const {
  Context& ctx = *static_cast<Context*>(frame.animation.context);
  auto w = static_cast<int>(width(frame));
  auto h = static_cast<int>(height(frame));

  //heat_t += freq;
  for (int x = 0; x < w; ++x) {

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < h; i++) {
      ctx.heat[i * w + x] = qsub8(
                              ctx.heat[i * w + x], random_(0, ((cooling * 10) / h) + 2));
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
    ctx.heat[x] = random_(160, 255);
  }
}

Color Flame::color(const Pixel& px) const {
  Context& ctx = *static_cast<Context*>(px.frame.animation.context);
  auto w = static_cast<int>(width(px.frame));
  auto h = static_cast<int>(height(px.frame));
  auto clr = Color(heatColor(ctx.heat[(h - static_cast<int>(px.coord.y)) * w +
                            static_cast<int>(px.coord.x)])).lightnessToAlpha();
  return clr;
}

void Flame::end(const Animation&) const {
}



} // namespace unisparks
