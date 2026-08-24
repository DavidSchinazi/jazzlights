#ifndef JL_EFFECT_RINGS_H
#define JL_EFFECT_RINGS_H

#include <algorithm>

#include "jazzlights/effect/palette.h"

namespace jazzlights {

struct RingsState {
  uint8_t startHue;
  Point origin;
  double maxDistance;
  bool backwards;
  uint8_t initialHue;
};

class Rings : public EffectWithPaletteAndState<RingsState> {
 public:
  Rings() = default;

  std::string effectNamePrefix(PatternBits /*pattern*/) const override { return "rings"; }

  void innerBegin(const Frame& frame, RingsState* state) const override {
    new (state) RingsState;  // Default-initialize the state.
    state->startHue = frame.predictableRandom->GetRandomByte();
    state->origin.x =
        frame.viewport.origin.x + frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.width);
    state->origin.y =
        frame.viewport.origin.y + frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.height);
    state->maxDistance = std::max({
        Distance(state->origin, LeftTop(frame)),
        Distance(state->origin, RightTop(frame)),
        Distance(state->origin, LeftBottom(frame)),
        Distance(state->origin, RightBottom(frame)),
    });
    state->backwards = frame.predictableRandom->GetRandomByte() & 1;
  }

  void innerRewind(const Frame& frame, RingsState* state) const override {
    // kPeriod needs to (almost) cleanly divide kEffectDurationMs to avoid visible resets when looping.
    static constexpr FrameTimeMs kPeriodMs = 1667;
    uint8_t hueOffset = 256 * frame.time / kPeriodMs;
    if (state->backwards) { hueOffset = 255 - hueOffset; }
    state->initialHue = state->startHue + hueOffset;
  }

  ColorWithPalette innerColor(const Frame& /*frame*/, const Pixel& px, RingsState* state) const override {
    const double d = Distance(px.coord, state->origin);
    return (state->initialHue + int32_t(255 * d / state->maxDistance)) % 255;
  }
};

}  // namespace jazzlights
#endif  // JL_EFFECT_RINGS_H
