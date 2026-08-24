#ifndef JL_EFFECT_SOUND_EFFECT_H
#define JL_EFFECT_SOUND_EFFECT_H

#include "jazzlights/audio/audio.h"
#include "jazzlights/effect/palette.h"
#include "jazzlights/render/fastled_wrapper.h"

#if JL_AUDIO_VISUALIZER

namespace jazzlights {

struct SoundState {
  Audio::VisualizerData audioData;
  float prevBands[Audio::kNumBands];
  CRGB brightestColor;
  bool isSquelched = false;
};

class SoundEffect : public EffectWithPaletteAndState<SoundState> {
 public:
  void InnerBegin(const Frame& frame, SoundState* state) const override;
  void InnerRewind(const Frame& frame, SoundState* state) const override;
  ColorWithPalette InnerColor(const Frame& frame, const Pixel& px, SoundState* state) const override;
  std::string EffectNamePrefix(PatternBits pattern) const override { return "sound"; }
  size_t ExtraContextSize(const Frame& frame) const override;

 private:
  CRGB* lastColors(SoundState* state) const { return reinterpret_cast<CRGB*>(state + 1); }
};

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER

#endif  // JL_EFFECT_SOUND_EFFECT_H
