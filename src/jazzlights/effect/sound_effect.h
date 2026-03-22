#ifndef JL_EFFECT_SOUND_EFFECT_H
#define JL_EFFECT_SOUND_EFFECT_H

#include "jazzlights/audio.h"
#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/palette.h"

namespace jazzlights {

struct SoundState {
  Audio::VisualizerData audioData;
  float prevBands[Audio::kNumBands];
  CRGB brightestColor;
};

class SoundEffect : public EffectWithPaletteAndState<SoundState> {
 public:
  void innerBegin(const Frame& frame, SoundState* state) const override;
  void innerRewind(const Frame& frame, SoundState* state) const override;
  ColorWithPalette innerColor(const Frame& frame, const Pixel& px, SoundState* state) const override;
  std::string effectNamePrefix(PatternBits pattern) const override { return "sound"; }
  size_t extraContextSize(const Frame& frame) const override;

 private:
  CRGB* lastColors(SoundState* state) const { return reinterpret_cast<CRGB*>(state + 1); }
};

}  // namespace jazzlights

#endif  // JL_EFFECT_SOUND_EFFECT_H
