#ifndef JL_EFFECT_FLAME_H
#define JL_EFFECT_FLAME_H

#include "jazzlights/effect/palette.h"

namespace jazzlights {

struct FlameState {
  TProgmemRGBPalette16 palette;
  uint8_t maxDim;
};

class Flame : public EffectWithPaletteXYIndexAndState<FlameState, uint8_t> {
 public:
  void InnerBegin(const Frame& frame, FlameState* state) const override;
  void InnerRewind(const Frame& frame, FlameState* state) const override;
  ColorWithPalette InnerColor(const Frame& frame, FlameState* state, const Pixel& px) const override;
  std::string EffectNamePrefix(PatternBits /*pattern*/) const override { return "flame"; }
};

}  // namespace jazzlights
#endif  // JL_EFFECT_FLAME_H
