#ifndef JL_EFFECT_SOUND_EFFECT_H
#define JL_EFFECT_SOUND_EFFECT_H

#include "jazzlights/audio.h"
#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/palette.h"

namespace jazzlights {

struct SoundState {
  Audio::VisualizerData audioData;
  CRGB brightestColor;
};

struct SoundPixelState {
  CRGB lastColor;
};

class SoundEffect : public EffectWithPaletteXYIndexAndState<SoundState, SoundPixelState> {
 public:
  void innerBegin(const Frame& frame, SoundState* state) const override;
  void innerRewind(const Frame& frame, SoundState* state) const override;
  ColorWithPalette innerColor(const Frame& frame, SoundState* state, const Pixel& px) const override;
  std::string effectNamePrefix(PatternBits pattern) const override { return "sound"; }
};

}  // namespace jazzlights

#endif  // JL_EFFECT_SOUND_EFFECT_H
