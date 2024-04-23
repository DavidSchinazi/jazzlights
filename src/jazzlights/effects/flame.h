#ifndef JL_EFFECTS_FLAME_H
#define JL_EFFECTS_FLAME_H

#include "jazzlights/palette.h"

namespace jazzlights {

struct FlameState {
  TProgmemRGBPalette16 palette;
  uint8_t maxDim;
};

class Flame : public EffectWithPaletteXYIndexAndState<FlameState, uint8_t> {
 public:
  void innerBegin(const Frame& frame, FlameState* state) const override;
  void innerRewind(const Frame& frame, FlameState* state) const override;
  ColorWithPalette innerColor(const Frame& frame, FlameState* state, const Pixel& px) const override;
  std::string effectNamePrefix(PatternBits /*pattern*/) const override { return "flame"; }
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_FLAME_H
