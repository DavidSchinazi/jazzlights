#include "jazzlights/effect/sound_effect.h"

#if JL_AUDIO_VISUALIZER

#include <cmath>
#include <cstring>

namespace jazzlights {

size_t SoundEffect::extraContextSize(const Frame& frame) const { return sizeof(CRGB) * frame.pixelCount; }

void SoundEffect::innerBegin(const Frame& frame, SoundState* state) const {
  Audio::Get().GetVisualizerData(&state->audioData);
  memcpy(state->prevBands, state->audioData.bands, sizeof(state->prevBands));

  OurColorPalette ocp = palette(frame);
  state->brightestColor = CRGB::Black;
  uint32_t maxSaturation = 0;
  for (int i = 0; i < 256; i += 8) {
    CRGB c = colorFromOurPalette(ocp, i);
    uint8_t cMax = max(c.r, max(c.g, c.b));
    uint8_t cMin = min(c.r, min(c.g, c.b));
    uint32_t saturation = cMax - cMin;
    if (saturation > maxSaturation) {
      maxSaturation = saturation;
      state->brightestColor = c;
    }
  }

  // Initialize per-pixel state
  memset(lastColors(state), 0, sizeof(CRGB) * frame.pixelCount);
}

void SoundEffect::innerRewind(const Frame& /*frame*/, SoundState* state) const {
  memcpy(state->prevBands, state->audioData.bands, sizeof(state->prevBands));
  Audio::Get().GetVisualizerData(&state->audioData);
  bool wasSquelched = state->isSquelched;
  state->isSquelched = (state->audioData.volume < 0.4f);
  if (state->isSquelched && !wasSquelched) {
    jll_info("Entering squelch mode (volume %f)", state->audioData.volume);
  } else if (!state->isSquelched && wasSquelched) {
    jll_info("Exiting squelch mode (volume %f)", state->audioData.volume);
  }
}

ColorWithPalette SoundEffect::innerColor(const Frame& frame, const Pixel& px, SoundState* state) const {
  // Squelch: Turn off LEDs if the overall volume is very low
  if (state->isSquelched) {
    CRGB* prevColors = lastColors(state);
    prevColors[px.cumulativeIndex] = CRGB::Black;
    return ColorWithPalette::OverrideColor(CRGB::Black);
  }

  CRGB color = CRGB::Black;
  float normalizedMag = 0;
  float transient = 0;

  // Map horizontal position to regions: Bass (0-20%), Mids (20-70%), Highs (70-100%)
  // Vary the regions slightly per strand
  double xOffset = (static_cast<double>(px.strand->index % 3) - 1.0) * 0.05;
  double xRel = (px.coord.x - frame.viewport.origin.x) / frame.viewport.size.width + xOffset;
  if (xRel < 0) xRel = 0;
  if (xRel > 0.999) xRel = 0.999;

  float magnitude = 0;
  float prevMagnitude = 0;
  OurColorPalette ocp = palette(frame);

  if (xRel < 0.2) {
    // Bass Region (0-20%): Bands 0-3
    double bassRel = xRel / 0.2;
    double xBand = bassRel * 3.0;
    int bandLow = static_cast<int>(xBand);
    int bandHigh = bandLow + 1;
    if (bandHigh > 3) bandHigh = 3;
    float frac = xBand - bandLow;
    magnitude = state->audioData.bands[bandLow] * (1.0f - frac) + state->audioData.bands[bandHigh] * frac;
    prevMagnitude = state->prevBands[bandLow] * (1.0f - frac) + state->prevBands[bandHigh] * frac;
    color = state->brightestColor;
  } else if (xRel < 0.7) {
    // Mids Region (20-70%): Bands 4-19
    double midsRel = (xRel - 0.2) / 0.5;
    double xBand = 4.0 + midsRel * 15.0;
    int bandLow = static_cast<int>(xBand);
    int bandHigh = bandLow + 1;
    if (bandHigh > 19) bandHigh = 19;
    float frac = xBand - bandLow;
    magnitude = state->audioData.bands[bandLow] * (1.0f - frac) + state->audioData.bands[bandHigh] * frac;
    prevMagnitude = state->prevBands[bandLow] * (1.0f - frac) + state->prevBands[bandHigh] * frac;
    // Vary the color index per strand
    uint8_t colorIdx = static_cast<uint8_t>(midsRel * 170.0) + (px.strand->index * 16);
    color = colorFromOurPalette(ocp, colorIdx);
  } else {
    // Highs Region (70-100%): Bands 20-31
    double highsRel = (xRel - 0.7) / 0.3;
    double xBand = 20.0 + highsRel * 11.0;
    int bandLow = static_cast<int>(xBand);
    int bandHigh = bandLow + 1;
    if (bandHigh > 31) bandHigh = 31;
    float frac = xBand - bandLow;
    magnitude = state->audioData.bands[bandLow] * (1.0f - frac) + state->audioData.bands[bandHigh] * frac;
    prevMagnitude = state->prevBands[bandLow] * (1.0f - frac) + state->prevBands[bandHigh] * frac;
    // Vary the color index per strand
    uint8_t colorIdx = 171 + static_cast<uint8_t>(highsRel * 84.0) + (px.strand->index * 16);
    color = colorFromOurPalette(ocp, colorIdx);
  }

  // Get magnitude for this band (expected to be roughly between agc_min and agc_max)
  float range = state->audioData.agc_max - state->audioData.agc_min;
  if (range < 1.0f) range = 1.0f;

  normalizedMag = (magnitude - state->audioData.agc_min) / range;
  if (normalizedMag < 0) normalizedMag = 0;
  if (normalizedMag > 1.0f) normalizedMag = 1.0f;
  // Boost smaller changes to make them more visible
  normalizedMag = powf(normalizedMag, 0.5f);  // Was 0.7f - more aggressive boost for low levels

  transient = (magnitude - prevMagnitude) / range;
  if (transient < 0) transient = 0;

  // Center-out vertical position (0 at center, 1 at edges)
  // Vary the center slightly per strand
  double yOffset = (static_cast<double>(px.strand->index % 5) - 2.0) * 0.1 * frame.viewport.size.height;
  double centerY = frame.viewport.origin.y + frame.viewport.size.height / 2.0 + yOffset;
  double yRel = 2.0 * fabs(px.coord.y - centerY) / frame.viewport.size.height;
  if (yRel > 1.0) yRel = 1.0;

  if (yRel < normalizedMag) {
    // Inside the bar: brightness proportional to magnitude
    uint8_t barBrightness = static_cast<uint8_t>(255.0f * normalizedMag);
    if (barBrightness < 48 && normalizedMag > 0.02f) barBrightness = 48;  // Was 32/0.05f
    color.nscale8_video(barBrightness);
  } else {
    // Background: dimmed version of the color, scaled by overall volume
    // Reduced background brightness for better contrast
    uint8_t backgroundBrightness = static_cast<uint8_t>(16.0f * powf(state->audioData.volume, 0.8f));  // Was 48.0f/0.7f
    color.nscale8_video(backgroundBrightness);
  }

  // Add Sparkles!
  uint32_t sparkleChance = 0;
  // Only sparkle on significant transients and when there's an actual signal in this band
  // and we're above the squelch threshold.
  if (state->audioData.volume > 0.5f && transient > 0.05f && normalizedMag > 0.2f) {
    // Audio-reactive sparkle based on transients, scaled by the magnitude of the band
    sparkleChance = static_cast<uint32_t>(transient * 10000.0f * normalizedMag);
  }

  if (sparkleChance > 0 &&
      frame.predictableRandom->GetRandomNumberBetween(0, 5000) < static_cast<int32_t>(sparkleChance)) {
    /*
    Remove the pure white case — replace color = CRGB::White with a brightened version of the existing color, e.g.
    color.maximizeBrightness() or just remove that branch entirely. Reduce the gray overlay — change CRGB(128, 128, 128)
    to something smaller like CRGB(64, 64, 64) or remove it entirely and just boost barBrightness. Reduce sparkle
    frequency — lower the multiplier 10000.0f on line 144 or raise the thresholds (0.5f volume, 0.05f transient, 0.2f
    normalizedMag`).
    */
    // Brighten or flash white
    if (frame.predictableRandom->GetRandomByte() > 128) {
      color.maximizeBrightness();  // Was CRGB::White - now a brightened version of the existing color
    } else {
      color += CRGB(128, 128, 128);  // Was 64
    }
  }

  CRGB* prevColors = lastColors(state);
  CRGB& lastColor = prevColors[px.cumulativeIndex];
  // Asymmetrical smoothing: faster on the way up, slower on the way down
  uint8_t blendAmount = 64;  // Was 48
  if (color.r > lastColor.r || color.g > lastColor.g || color.b > lastColor.b) {
    blendAmount = 192;  // Was 128
  }
  color = nblend(lastColor, color, blendAmount);
  lastColor = color;

  return ColorWithPalette::OverrideColor(color);
}
}  // namespace jazzlights

#endif  // JL_AUDIO_VISUALIZER
