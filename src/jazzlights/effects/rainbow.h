#ifndef JL_EFFECTS_RAINBOW_H
#define JL_EFFECTS_RAINBOW_H

#include <algorithm>

#include "jazzlights/effects/functional.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

class Rainbow : public Effect {
 public:
  Rainbow() = default;

  std::string effectName(PatternBits /*pattern*/) const override { return "rainbow"; }

  size_t contextSize(const Frame& /*frame*/) const override { return sizeof(RainbowState); }

  void begin(const Frame& frame) const override {
    RainbowState* state = reinterpret_cast<RainbowState*>(frame.context);
    state->startHue = frame.predictableRandom->GetRandomByte();
    state->origin.x =
        frame.viewport.origin.x + frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.width);
    state->origin.y =
        frame.viewport.origin.y + frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.height);
    state->maxDistance = std::max({
        distance(state->origin, lefttop(frame)),
        distance(state->origin, righttop(frame)),
        distance(state->origin, leftbottom(frame)),
        distance(state->origin, rightbottom(frame)),
    });
    state->backwards = frame.predictableRandom->GetRandomByte() & 1;
  }

  void rewind(const Frame& frame) const override {
    RainbowState* state = reinterpret_cast<RainbowState*>(frame.context);
    uint8_t hueOffset = 256 * frame.time / 1500;
    if (state->backwards) { hueOffset = 255 - hueOffset; }
    state->initialHue = state->startHue + hueOffset;
  }

  void afterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<RainbowState>::value, "RainbowState must be trivially destructible");
  }

  Color color(const Frame& frame, const Pixel& px) const override {
    RainbowState* state = reinterpret_cast<RainbowState*>(frame.context);
    const double d = distance(px.coord, state->origin);
    const uint8_t hue = (state->initialHue + int32_t(255 * d / state->maxDistance)) % 255;
    return HslColor(hue, 240, 255);
  }

 private:
  struct RainbowState {
    uint8_t startHue;
    Point origin;
    double maxDistance;
    bool backwards;
    uint8_t initialHue;
  };
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_RAINBOW_H
