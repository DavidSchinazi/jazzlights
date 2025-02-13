#ifndef JL_EFFECT_GLOW_H
#define JL_EFFECT_GLOW_H
#include "jazzlights/effect/functional.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

inline FunctionalEffect glow(CRGB color, const std::string& name) {
  return effect(name, [color](const Frame& frame) {
    constexpr uint32_t period = 2500;
    constexpr uint32_t half_low_time = 10;
    constexpr uint32_t half_high_time = 400;
    constexpr uint32_t half_period = period / 2;
    uint32_t time_in_period = frame.time % period;
    if (time_in_period > half_period) { time_in_period = period - time_in_period; }
    constexpr uint8_t min_intensity = 25;
    constexpr uint8_t max_intensity = 255;
    constexpr uint8_t intensity_delta = max_intensity - min_intensity;
    uint8_t intensity;
    if (time_in_period <= half_low_time) {
      intensity = min_intensity;
    } else if (time_in_period > half_low_time && time_in_period < (half_period - half_high_time)) {
      const uint32_t ramp_time = time_in_period - half_low_time;
      constexpr uint32_t ramp_length = half_period - (half_low_time + half_high_time);
      intensity = (ramp_time * intensity_delta / ramp_length) + min_intensity;
    } else {
      intensity = max_intensity;
    }
    const CRGB faded_color = FadeColor(color, intensity);
    return [faded_color](const Pixel& /*pt*/) -> CRGB { return faded_color; };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECT_GLOW_H
