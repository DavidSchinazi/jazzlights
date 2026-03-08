#include "jazzlights/effect/sound_effect.h"

#if JL_AUDIO_VISUALIZER

#include <cmath>

namespace jazzlights {

void SoundEffect::innerBegin(const Frame& /*frame*/, SoundState* state) const {
  Audio::Get().GetVisualizerData(&state->audioData);
  state->currentVolume = 0;
}

void SoundEffect::innerRewind(const Frame& /*frame*/, SoundState* state) const {
  Audio::Get().GetVisualizerData(&state->audioData);

  // Calculate average normalized magnitude for currentVolume
  float range = state->audioData.agc_max - state->audioData.agc_min;
  if (range < 1.0f) range = 1.0f;

  float totalNormMag = 0;
  for (int i = 0; i < Audio::kNumBands; i++) {
    float norm = (state->audioData.bands[i] - state->audioData.agc_min) / range;
    if (norm < 0) norm = 0;
    if (norm > 1.0f) norm = 1.0f;
    totalNormMag += norm;
  }
  state->currentVolume = totalNormMag / Audio::kNumBands;
}

ColorWithPalette SoundEffect::innerColor(const Frame& frame, const Pixel& px, SoundState* state) const {
  // Map horizontal position to frequency band (0 to 31)
  double xRel = (px.coord.x - frame.viewport.origin.x) / frame.viewport.size.width;
  if (xRel < 0) xRel = 0;
  if (xRel > 0.999) xRel = 0.999;

  // Use interpolation between bands for smoother visuals
  double xBand = xRel * (Audio::kNumBands - 1);
  int bandLow = static_cast<int>(xBand);
  int bandHigh = bandLow + 1;
  if (bandHigh >= Audio::kNumBands) bandHigh = Audio::kNumBands - 1;
  float frac = xBand - bandLow;

  float magnitude = state->audioData.bands[bandLow] * (1.0f - frac) + state->audioData.bands[bandHigh] * frac;

  // Get magnitude for this band (expected to be roughly between agc_min and agc_max)
  float range = state->audioData.agc_max - state->audioData.agc_min;
  if (range < 1.0f) range = 1.0f;

  float normalizedMag = (magnitude - state->audioData.agc_min) / range;
  if (normalizedMag < 0) normalizedMag = 0;
  if (normalizedMag > 1.0f) normalizedMag = 1.0f;

  // Center-out vertical position (0 at center, 1 at edges)
  double centerY = frame.viewport.origin.y + frame.viewport.size.height / 2.0;
  double yRel = 2.0 * fabs(px.coord.y - centerY) / frame.viewport.size.height;
  if (yRel > 1.0) yRel = 1.0;

  uint8_t colorIdx = static_cast<uint8_t>(xRel * 255);
  OurColorPalette ocp = palette(frame);
  CRGB color = colorFromOurPalette(ocp, colorIdx);

  if (yRel < normalizedMag) {
    // Inside the bar: bright, slightly pulsing with band magnitude
    uint8_t barBrightness = 192 + static_cast<uint8_t>(63.0f * normalizedMag);
    color.nscale8_video(barBrightness);
    return ColorWithPalette::OverrideColor(color);
  } else {
    // Background: dimmed version of the frequency color, scaled by overall volume
    uint8_t backgroundBrightness = 12 + static_cast<uint8_t>(64.0f * state->currentVolume);
    color.nscale8_video(backgroundBrightness);
    return ColorWithPalette::OverrideColor(color);
  }
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER
