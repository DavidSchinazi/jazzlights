#include "jazzlights/effect/flame.h"

#include <assert.h>

#include "jazzlights/config.h"
#include "jazzlights/player.h"

namespace jazzlights {

namespace {
#if JL_IS_CONFIG(HAT) || JL_IS_CONFIG(SHOE)
constexpr uint8_t kIgnitionMin = 0;
constexpr uint8_t kIgnitionMax = 15;
#else   // HAT
constexpr uint8_t kIgnitionMin = 160;
constexpr uint8_t kIgnitionMax = 255;
#endif  // HAT
}  // namespace

void Flame::innerBegin(const Frame& f, FlameState* state) const {
  OurColorPalette p = palette(f);
  if (p == OCPlava) {  // Lava is similar to heat, but for this pattern heat looks much better.
    p = OCPheat;
  }
  memcpy(state->palette, FastLEDPaletteFromOurColorPalette(p), sizeof(state->palette));
  state->palette[0] = CRGB::Black;
  if (h(f) > 8) {
    state->maxDim = 1012 / h(f) + 12;
  } else {
    state->maxDim = 128;
  }
}

void Flame::innerRewind(const Frame& f, FlameState* state) const {
  for (size_t x = 0; x < w(f); x++) {
    // Step 1.  Cool down every cell a little
    for (size_t y = 0; y < h(f); y++) {
      ps(f, x, y) = qsub8(ps(f, x, y), f.predictableRandom->GetRandomNumberBetween(0, state->maxDim));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (size_t y2 = h(f) - 1; y2 >= 3; y2--) {
      ps(f, x, y2) = (static_cast<uint16_t>(ps(f, x, y2 - 1)) + static_cast<uint16_t>(ps(f, x, y2 - 2)) +
                      static_cast<uint16_t>(ps(f, x, y2 - 3))) /
                     3;
    }
    if (h(f) > 2) {
      ps(f, x, 2) = (static_cast<uint16_t>(ps(f, x, 1)) + static_cast<uint16_t>(ps(f, x, 0))) / 2;
      ps(f, x, 1) = ps(f, x, 0);
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    ps(f, x, 0) = f.predictableRandom->GetRandomNumberBetween(kIgnitionMin, kIgnitionMax);
  }
}

ColorWithPalette Flame::innerColor(const Frame& f, FlameState* state, const Pixel& /*px*/) const {
  const uint8_t temperature = ps(f, x(f), h(f) - 1 - y(f));
  return ColorWithPalette::OverrideColor(ColorFromPalette(state->palette, temperature,
                                                          /*brightness=*/255, LINEARBLEND_NOWRAP));
}

}  // namespace jazzlights
