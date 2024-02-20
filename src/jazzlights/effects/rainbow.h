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
    new (state(frame)) RainbowState;  // Default-initialize the state.
    state(frame)->startHue = frame.predictableRandom->GetRandomByte();
    state(frame)->origin.x =
        frame.viewport.origin.x + frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.width);
    state(frame)->origin.y =
        frame.viewport.origin.y + frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.height);
    state(frame)->maxDistance = std::max({
        distance(state(frame)->origin, lefttop(frame)),
        distance(state(frame)->origin, righttop(frame)),
        distance(state(frame)->origin, leftbottom(frame)),
        distance(state(frame)->origin, rightbottom(frame)),
    });
    state(frame)->backwards = frame.predictableRandom->GetRandomByte() & 1;
  }

  void rewind(const Frame& frame) const override {
    uint8_t hueOffset = 256 * frame.time / 1500;
    if (state(frame)->backwards) { hueOffset = 255 - hueOffset; }
    state(frame)->initialHue = state(frame)->startHue + hueOffset;
  }

  void afterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<RainbowState>::value, "RainbowState must be trivially destructible");
  }

  Color color(const Frame& frame, const Pixel& px) const override {
    const double d = distance(px.coord, state(frame)->origin);
    const uint8_t hue = (state(frame)->initialHue + int32_t(255 * d / state(frame)->maxDistance)) % 255;
    return RgbColorFromHsv(hue, 240, 255);
  }

 private:
  struct RainbowState {
    uint8_t startHue;
    Point origin;
    double maxDistance;
    bool backwards;
    uint8_t initialHue;
  };
  RainbowState* state(const Frame& frame) const {
    static_assert(alignof(RainbowState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<RainbowState*>(frame.context);
  }
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_RAINBOW_H
