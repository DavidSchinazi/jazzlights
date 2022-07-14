#ifndef UNISPARKS_EFFECTS_GLITTER_H
#define UNISPARKS_EFFECTS_GLITTER_H

#include "unisparks/effect.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

class Glitter : public Effect {
 public:
  Glitter() = default;

  std::string effectName(PatternBits /*pattern*/) const override {
    return "glitter";
  }

  size_t contextSize(const Animation&) const override { return sizeof(GlitterState); }

  void begin(const Frame& frame) const override {
    GlitterState* state = reinterpret_cast<GlitterState*>(frame.animation.context);
    state->startHue = frame.predictableRandom->GetRandomByte();
    state->backwards = frame.predictableRandom->GetRandomByte() & 1;
  }

  void rewind(const Frame& frame) const override {
    GlitterState* state = reinterpret_cast<GlitterState*>(frame.animation.context);
    uint8_t hueOffset = 256 * frame.time / kEffectDuration;
    if (state->backwards) {
      hueOffset = 255 - hueOffset;
    }
    state->hue = state->startHue + hueOffset;
  }

  Color color(const Pixel& pixel) const override {
    GlitterState* state = reinterpret_cast<GlitterState*>(pixel.frame.animation.context);
    return HslColor(state->hue, 255, pixel.frame.predictableRandom->GetRandomByte());
  }

 private:
  struct GlitterState {
    uint8_t startHue;
    bool backwards;
    uint8_t hue;
  };
};

}  // namespace unisparks
#endif /* UNISPARKS_EFFECTS_GLITTER_H */
