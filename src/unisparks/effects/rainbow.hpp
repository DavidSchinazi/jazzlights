#ifndef UNISPARKS_EFFECTS_RAINBOW_H
#define UNISPARKS_EFFECTS_RAINBOW_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {
#if 0
class Rainbow : public Effect {
 public:
  Rainbow() = default;

  std::string effectName(PatternBits /*pattern*/) const override {
    return "rainbow";
  }

  size_t contextSize(const Frame& /*frame*/) const override { return sizeof(RainbowState); }

  void begin(const Frame& frame) const override {
    RainbowState* state = reinterpret_cast<RainbowState*>(frame.context);
    state->startHue = frame.predictableRandom->GetRandomByte();
    state->backwards = frame.predictableRandom->GetRandomByte() & 1;
  }

  void rewind(const Frame& frame) const override {
    RainbowState* state = reinterpret_cast<RainbowState*>(frame.context);
    uint8_t hueOffset = 256 * frame.time / 1500;
    if (state->backwards) {
      hueOffset = 255 - hueOffset;
    }
    state->hue = state->startHue + hueOffset;
  }

  Color color(const Frame& frame, const Pixel& /*px*/) const override {
    RainbowState* state = reinterpret_cast<RainbowState*>(frame.context);
    return HslColor(state->hue, 255, frame.predictableRandom->GetRandomByte());
  }

 private:
  struct RainbowState {
    uint8_t startHue;
    Point origin;
    uint8_t initialHue;
    bool backwards;
    uint8_t hue;
  };
};
#endif

auto rainbow = []() {
  return effect("rainbow", [](const Frame & frame) {
    uint8_t initial_hue = 255 - 255 * frame.time / 1500;
    double maxd = distance(center(frame), lefttop(frame));

    return [ = ](const Pixel& px) -> Color {
      double d = distance(px.coord, center(frame));
      uint8_t hue = (initial_hue + int32_t(255 * d / maxd)) % 255;
      return HslColor(hue, 240, 255);
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_RAINBOW_H */

