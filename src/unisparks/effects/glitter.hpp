#ifndef UNISPARKS_EFFECTS_GLITTER_H
#define UNISPARKS_EFFECTS_GLITTER_H

#include "unisparks/effect.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

class Glitter : public Effect {
 public:
  Glitter() = default;

  std::string name() const override {
    return "glitter";
  }

  size_t contextSize(const Animation&) const override { return 0; }

  void begin(const Frame& frame) override {
    startHue_ = frame.player->predictableRandom().GetRandomByte();
    backwards_ = frame.player->predictableRandom().GetRandomByte() & 1;
  }

  void rewind(const Frame& frame) override {
    uint8_t hueOffset = 256 * frame.time / kEffectDuration;
    if (backwards_) {
      hueOffset = 255 - hueOffset;
    }
    hue_ = startHue_ + hueOffset;
  }

  Color color(const Pixel& pixel) const override {
    return HslColor(hue_, 255, pixel.frame.player->predictableRandom().GetRandomByte());
  }

 private:
  uint8_t startHue_;
  bool backwards_;
  uint8_t hue_;
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_GLITTER_H */
