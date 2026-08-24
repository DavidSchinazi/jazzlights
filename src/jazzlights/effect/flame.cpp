#include "jazzlights/effect/flame.h"

#include "jazzlights/render/player.h"
#include "jazzlights/util/config.h"

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

void Flame::InnerBegin(const Frame& f, FlameState* state) const {
  OurColorPalette p = palette(f);
  if (p == OCPlava) {  // Lava is similar to heat, but for this pattern heat looks much better.
    p = OCPheat;
  }
  memcpy(state->palette, FastLEDPaletteFromOurColorPalette(p), sizeof(state->palette));
  state->palette[0] = CRGB::Black;
  if (H(f) > 8) {
    state->maxDim = 1012 / H(f) + 12;
  } else {
    state->maxDim = 128;
  }
}

void Flame::InnerRewind(const Frame& f, FlameState* state) const {
  for (size_t x = 0; x < W(f); x++) {
    // Step 1.  Cool down every cell a little
    for (size_t y = 0; y < H(f); y++) {
      Ps(f, x, y) = qsub8(Ps(f, x, y), f.predictableRandom->GetRandomNumberBetween(0, state->maxDim));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (size_t y2 = H(f) - 1; y2 >= 3; y2--) {
      Ps(f, x, y2) = (static_cast<uint16_t>(Ps(f, x, y2 - 1)) + static_cast<uint16_t>(Ps(f, x, y2 - 2)) +
                      static_cast<uint16_t>(Ps(f, x, y2 - 3))) /
                     3;
    }
    if (H(f) > 2) {
      Ps(f, x, 2) = (static_cast<uint16_t>(Ps(f, x, 1)) + static_cast<uint16_t>(Ps(f, x, 0))) / 2;
      Ps(f, x, 1) = Ps(f, x, 0);
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    Ps(f, x, 0) = f.predictableRandom->GetRandomNumberBetween(kIgnitionMin, kIgnitionMax);
  }
}

ColorWithPalette Flame::InnerColor(const Frame& f, FlameState* state, const Pixel& /*px*/) const {
  const uint8_t temperature = Ps(f, X(f), H(f) - 1 - Y(f));
  return ColorWithPalette::OverrideColor(::ColorFromPalette(state->palette, temperature,
                                                            /*brightness=*/255, LINEARBLEND_NOWRAP));
}

}  // namespace jazzlights
