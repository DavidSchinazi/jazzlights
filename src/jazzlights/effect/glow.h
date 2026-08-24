#ifndef JL_EFFECT_GLOW_H
#define JL_EFFECT_GLOW_H
#include "jazzlights/effect/functional.h"

namespace jazzlights {

inline FunctionalEffect Glow(CRGB color, const std::string& name) {
  return FunctionalEffectFrom(name, [color](const Frame& frame) {
    constexpr uint32_t kPeriod = 2500;
    constexpr uint32_t kHalfLowTime = 10;
    constexpr uint32_t kHalfHighTime = 400;
    constexpr uint32_t kHalfPeriod = kPeriod / 2;
    uint32_t timeInPeriod = frame.time % kPeriod;
    if (timeInPeriod > kHalfPeriod) { timeInPeriod = kPeriod - timeInPeriod; }
    constexpr uint8_t kMinIntensity = 25;
    constexpr uint8_t kMaxIntensity = 255;
    constexpr uint8_t kIntensityDelta = kMaxIntensity - kMinIntensity;
    uint8_t intensity;
    if (timeInPeriod <= kHalfLowTime) {
      intensity = kMinIntensity;
    } else if (timeInPeriod > kHalfLowTime && timeInPeriod < (kHalfPeriod - kHalfHighTime)) {
      const uint32_t rampTime = timeInPeriod - kHalfLowTime;
      constexpr uint32_t kRampLength = kHalfPeriod - (kHalfLowTime + kHalfHighTime);
      intensity = (rampTime * kIntensityDelta / kRampLength) + kMinIntensity;
    } else {
      intensity = kMaxIntensity;
    }
    const CRGB fadedColor = FadeColor(color, intensity);
    return [fadedColor](const Pixel& /*pt*/) -> CRGB { return fadedColor; };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECT_GLOW_H
