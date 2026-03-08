#include "jazzlights/effect/sound_effect.h"

#if JL_AUDIO_VISUALIZER

namespace jazzlights {

void SoundEffect::innerBegin(const Frame& /*frame*/, SoundState* state) const {
  Audio::Get().GetVisualizerData(&state->audioData);
}

void SoundEffect::innerRewind(const Frame& /*frame*/, SoundState* state) const {
  Audio::Get().GetVisualizerData(&state->audioData);
}

ColorWithPalette SoundEffect::innerColor(const Frame& frame, const Pixel& px, SoundState* state) const {
  // Map horizontal position to frequency band (0 to 31)
  double xRel = (px.coord.x - frame.viewport.origin.x) / frame.viewport.size.width;
  if (xRel < 0) xRel = 0;
  if (xRel > 0.999) xRel = 0.999;
  int bandIdx = static_cast<int>(xRel * Audio::kNumBands);

  // Get magnitude for this band (expected to be roughly between agc_min and agc_max)
  float magnitude = state->audioData.bands[bandIdx];
  float range = state->audioData.agc_max - state->audioData.agc_min;
  if (range < 1.0f) range = 1.0f;

  float normalizedMag = (magnitude - state->audioData.agc_min) / range;
  if (normalizedMag < 0) normalizedMag = 0;
  if (normalizedMag > 1.0f) normalizedMag = 1.0f;

  // Vertical position for bar height
  double yRel = 1.0 - (px.coord.y - frame.viewport.origin.y) / frame.viewport.size.height;
  if (yRel < 0) yRel = 0;
  if (yRel > 1.0) yRel = 1.0;

  uint8_t colorIdx = static_cast<uint8_t>(xRel * 255);

  if (yRel < normalizedMag) {
    // Inside the bar
    return ColorWithPalette(colorIdx);
  } else if (state->audioData.beat && yRel < normalizedMag + 0.1) {
    // Small highlight on beat
    return ColorWithPalette(colorIdx + 128);
  }

  return ColorWithPalette::OverrideColor(CRGB::Black);
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER
