#include "jazzlights/effect/sound_effect.h"

#if JL_AUDIO_VISUALIZER

#include <cmath>

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

  if (yRel < normalizedMag) {
    // Inside the bar
    if (state->audioData.beat) {
      // Highlight on beat
      return ColorWithPalette(colorIdx + 128);
    }
    return ColorWithPalette(colorIdx);
  } else {
    // Background: dimmed version of the frequency color
    // If it's a beat, make the background brighter
    CRGB color = colorFromOurPalette(ocp, colorIdx);
    uint8_t backgroundBrightness = state->audioData.beat ? 48 : 12;
    color.nscale8_video(backgroundBrightness);
    return ColorWithPalette::OverrideColor(color);
  }
}

}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER
