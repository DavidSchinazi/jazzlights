#include "jazzlights/effects/flame.h"

#include <assert.h>

#include "jazzlights/config.h"
#include "jazzlights/player.h"
#include "jazzlights/util/color.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

namespace {
#if JL_IS_CONFIG(CAPTAIN_HAT)
constexpr uint8_t kIgnitionMin = 0;
constexpr uint8_t kIgnitionMax = 15;
#else   // CAPTAIN_HAT
constexpr uint8_t kIgnitionMin = 160;
constexpr uint8_t kIgnitionMax = 255;
#endif  // CAPTAIN_HAT
}  // namespace

RgbaColor heatColor(uint8_t temperature) {
  uint8_t r, g, b;
  // return rgb(temperature,temperature,temperature);

  // Scale 'heat' down from 0-255 to 0-191,
  // which can then be easily divided into three
  // equal 'thirds' of 64 units each.
  uint8_t t192 = scale8_video(temperature, 192);

  // calculate a value that ramps up from
  // zero to 255 in each 'third' of the scale.
  uint8_t heatramp = t192 & 0x3F;  // 0..63
  heatramp <<= 2;                  // scale up to 0..252

  // now figure out which third of the spectrum we're in:
  if (t192 & 0x80) {
    // we're in the hottest third
    r = 255;       // full red
    g = 255;       // full green
    b = heatramp;  // ramp up blue

  } else if (t192 & 0x40) {
    // we're in the middle third
    r = 255;       // full red
    g = heatramp;  // ramp up green
    b = 0;         // no blue

  } else {
    // we're in the coolest third
    r = heatramp;  // ramp up red
    g = 0;         // no green
    b = 0;         // no blue
  }

  return RgbaColor(r, g, b);
}

void Flame::innerBegin(const Frame& /*frame*/, FlameState* state) const {
  memset(&ps(0, 0), 0, sizeof(uint8_t) * w() * h());
  state->maxDim = 1012 / h() + 12;
}

void Flame::innerRewind(const Frame& frame, FlameState* state) const {
  for (size_t x = 0; x < w(); x++) {
    // Step 1.  Cool down every cell a little
    for (size_t y = 0; y < h(); y++) {
      ps(x, y) = qsub8(ps(x, y), frame.predictableRandom->GetRandomNumberBetween(0, state->maxDim));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (size_t y2 = h() - 1; y2 >= 3; y2--) {
      ps(x, y2) = (static_cast<uint16_t>(ps(x, y2 - 1)) + static_cast<uint16_t>(ps(x, y2 - 2)) +
                   static_cast<uint16_t>(ps(x, y2 - 3))) /
                  3;
    }
    ps(x, 2) = (static_cast<uint16_t>(ps(x, 1)) + static_cast<uint16_t>(ps(x, 0))) / 2;
    ps(x, 1) = ps(x, 0);

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    ps(x, 0) = frame.predictableRandom->GetRandomNumberBetween(kIgnitionMin, kIgnitionMax);
  }
}

Color Flame::innerColor(const Frame& /*frame*/, FlameState* /*state*/, const Pixel& /*px*/) const {
  return Color(heatColor(ps(x(), h() - 1 - y()))).lightnessToAlpha();
}

}  // namespace jazzlights
