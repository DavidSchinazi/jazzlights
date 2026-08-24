#ifndef JL_EFFECT_GLITTER_H
#define JL_EFFECT_GLITTER_H

#include "jazzlights/effect/effect.h"

namespace jazzlights {

class Glitter : public Effect {
 public:
  Glitter() = default;

  std::string EffectName(PatternBits /*pattern*/) const override { return "glitter"; }

  size_t ContextSize(const Frame& /*frame*/) const override { return sizeof(GlitterState); }

  void Begin(const Frame& frame) const override {
    new (State(frame)) GlitterState;  // Default-initialize the state.
    State(frame)->startHue = frame.predictableRandom->GetRandomByte();
    State(frame)->backwards = frame.predictableRandom->GetRandomByte() & 1;
  }

  void Rewind(const Frame& frame) const override {
    uint8_t hueOffset = 256 * frame.time / kEffectDurationMs;
    if (State(frame)->backwards) { hueOffset = 255 - hueOffset; }
    State(frame)->hue = State(frame)->startHue + hueOffset;
  }

  void AfterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<GlitterState>::value, "GlitterState must be trivially destructible");
  }

  CRGB Color(const Frame& frame, const Pixel& /*px*/) const override {
    return CHSV(State(frame)->hue, 255, frame.predictableRandom->GetRandomByte());
  }

 private:
  struct GlitterState {
    uint8_t startHue;
    bool backwards;
    uint8_t hue;
  };
  GlitterState* State(const Frame& frame) const {
    static_assert(alignof(GlitterState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<GlitterState*>(frame.context);
  }
};

}  // namespace jazzlights
#endif  // JL_EFFECT_GLITTER_H
