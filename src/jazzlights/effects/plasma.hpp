#ifndef JAZZLIGHTS_EFFECTS_PLASMA_H
#define JAZZLIGHTS_EFFECTS_PLASMA_H

#include "jazzlights/effect.hpp"
#include "jazzlights/palette.h"
#include "jazzlights/util/math.hpp"

namespace jazzlights {

// SpinPlasma orginally inspired from RGBShadesAudio's audioPlasma.
// https://github.com/macetech/RGBShadesAudio/blob/master/effects.h

struct SpinPlasmaState {
  float plasmaCenterX;
  float plasmaCenterY;
  float xMultiplier;
  float yMultiplier;
  float rotationCenterX;
  float rotationCenterY;
};

class SpinPlasma : public EffectWithPaletteAndState<SpinPlasmaState> {
public:
  std::string effectNamePrefix(PatternBits /*pattern*/) const override { return "sp"; }
  ColorWithPalette innerColor(const Frame& /*frame*/,
                              const Pixel& px,
                              SpinPlasmaState* state) const override {
    using namespace internal;
    return
      sin8(sqrt(square((static_cast<float>(px.coord.x) - state->plasmaCenterX) *
                        state->xMultiplier) +
                square((static_cast<float>(px.coord.y) - state->plasmaCenterY) *
                        state->yMultiplier)));
  }
  void innerBegin(const Frame& frame, SpinPlasmaState* state) const override {
    const float multiplier = frame.predictableRandom->GetRandomNumberBetween(100, 500);
    state->xMultiplier = multiplier / frame.viewport.size.width;
    state->yMultiplier = multiplier / frame.viewport.size.height;
    state->rotationCenterX =
      frame.viewport.origin.x +
      frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.width);
    state->rotationCenterY =
      frame.viewport.origin.y +
      frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.height);
  }
  void innerRewind(const Frame& frame, SpinPlasmaState* state) const override {
    using namespace internal;
    const uint8_t offset = 30 * frame.time / 255;
    state->plasmaCenterX =
      state->rotationCenterX +
      (static_cast<float>(cos8(offset)) - 127.0) / (state->xMultiplier * 2.0);
    state->plasmaCenterY =
      state->rotationCenterY +
      (static_cast<float>(sin8(offset)) - 127.0) / (state->yMultiplier * 2.0);
  }
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_EFFECTS_PLASMA_H
