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
    GlitterState* state = reinterpret_cast<GlitterState*>(frame.context);
    state->startHue = frame.predictableRandom->GetRandomByte();
    state->backwards = frame.predictableRandom->GetRandomByte() & 1;
  }

  void rewind(const Frame& frame) const override {
    GlitterState* state = reinterpret_cast<GlitterState*>(frame.context);
    uint8_t hueOffset = 256 * frame.time / kEffectDuration;
    if (state->backwards) { hueOffset = 255 - hueOffset; }
    state->hue = state->startHue + hueOffset;
  }

  Color color(const Frame& frame, const Pixel& /*px*/) const override {
    GlitterState* state = reinterpret_cast<GlitterState*>(frame.context);
    return HslColor(state->hue, 255, frame.predictableRandom->GetRandomByte());
  }

 private:
  struct GlitterState {
    uint8_t startHue;
    bool backwards;
    uint8_t hue;
  };
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_GLITTER_H
