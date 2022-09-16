#ifndef JAZZLIGHTS_EFFECTS_HIPHOTIC_H
#define JAZZLIGHTS_EFFECTS_HIPHOTIC_H

#include "jazzlights/effect.h"
#include "jazzlights/palette.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

// Hiphotic orginally inspired from Sound Reactive WLED.
// https://github.com/scottrbailey/WLED-Utils/blob/main/effects_sr.md
// https://github.com/atuline/WLED/blob/master/wled00/FX.cpp

struct HiphoticState {
  uint8_t offsetScale;
  float xScale;
  float yScale;
  int offset;
};

class Hiphotic : public EffectWithPaletteAndState<HiphoticState> {
public:
  std::string effectNamePrefix(PatternBits /*pattern*/) const override { return "hiphotic"; }
  ColorWithPalette innerColor(const Frame& frame,
                              const Pixel& px,
                              HiphoticState* state) const override {
    using namespace internal;
    const float x = (px.coord.x - frame.viewport.origin.x) / frame.viewport.size.width;
    const float y = (px.coord.y - frame.viewport.origin.y) / frame.viewport.size.height;
    return
      sin8(cos8(x * state->xScale + state->offset / 3) +
           sin8(y * state->yScale + state->offset / 4) +
           state->offset);
  }
  void innerBegin(const Frame& frame, HiphoticState* state) const override {
    state->offsetScale = frame.predictableRandom->GetRandomNumberBetween(6, 10);
    state->xScale = frame.predictableRandom->GetRandomDoubleBetween(100.0, 200.0);
    state->yScale = frame.predictableRandom->GetRandomDoubleBetween(100.0, 200.0);
  }
  void innerRewind(const Frame& frame, HiphoticState* state) const override {
    state->offset = frame.time / state->offsetScale;
  }
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_EFFECTS_HIPHOTIC_H
