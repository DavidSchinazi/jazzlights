#ifndef JL_EFFECTS_FLAME_H
#define JL_EFFECTS_FLAME_H

#include "jazzlights/effects/functional.h"
namespace jazzlights {

struct FlameState {
  uint8_t maxDim;
};

class Flame : public XYIndexStateEffect<FlameState, uint8_t> {
 public:
  void innerBegin(const Frame& frame, FlameState* state) const override;
  void innerRewind(const Frame& frame, FlameState* state) const override;
  CRGB innerColor(const Frame& frame, FlameState* state, const Pixel& px) const override;
  std::string effectName(PatternBits /*pattern*/) const override { return "flame"; }
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_FLAME_H
