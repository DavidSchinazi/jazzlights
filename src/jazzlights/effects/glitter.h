#ifndef JL_EFFECTS_GLITTER_H
#define JL_EFFECTS_GLITTER_H

#include "jazzlights/effect.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

class Glitter : public Effect {
 public:
  Glitter() = default;

  std::string effectName(PatternBits /*pattern*/) const override { return "glitter"; }

  size_t contextSize(const Frame& /*frame*/) const override { return sizeof(GlitterState); }

  void begin(const Frame& frame) const override {
    new (state(frame)) GlitterState;  // Default-initialize the state.
    state(frame)->startHue = frame.predictableRandom->GetRandomByte();
    state(frame)->backwards = frame.predictableRandom->GetRandomByte() & 1;
  }

  void rewind(const Frame& frame) const override {
    uint8_t hueOffset = 256 * frame.time / kEffectDuration;
    if (state(frame)->backwards) { hueOffset = 255 - hueOffset; }
    state(frame)->hue = state(frame)->startHue + hueOffset;
  }

  void afterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<GlitterState>::value, "GlitterState must be trivially destructible");
  }

  Color color(const Frame& frame, const Pixel& /*px*/) const override {
    return HslColor(state(frame)->hue, 255, frame.predictableRandom->GetRandomByte());
  }

 private:
  struct GlitterState {
    uint8_t startHue;
    bool backwards;
    uint8_t hue;
  };
  GlitterState* state(const Frame& frame) const {
    static_assert(alignof(GlitterState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<GlitterState*>(frame.context);
  }
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_GLITTER_H
