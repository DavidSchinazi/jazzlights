#ifndef JL_WLED_MAPPING_H
#define JL_WLED_MAPPING_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "jazzlights/util/types.h"

// Conversions between WLED's state and JazzLights pattern bits, for the WLED usermod in extras/wled. See
// extras/docs/WLED.md. Nothing here depends on WLED's headers, so it builds and is tested natively.

namespace jazzlights {

struct WLEDColor {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  bool operator==(const WLEDColor& other) const { return r == other.r && g == other.g && b == other.b; }
  bool operator!=(const WLEDColor& other) const { return !(*this == other); }
};

// JazzLights palettes, numbered like ApplyPalette() numbers them.
enum class PatternPalette : uint8_t {
  kHeat = 0,
  kLava = 1,
  kOcean = 2,
  kCloud = 3,
  kParty = 4,
  kForest = 5,
  kRainbow = 6,
};
inline constexpr uint8_t kNumPatternPalettes = 7;

const char* PatternPaletteName(PatternPalette palette);
// The 16 colors of the FastLED palette that JazzLights renders with.
const std::array<WLEDColor, 16>& PatternPaletteColors(PatternPalette palette);

// The JazzLights effects that a pattern can select, as decided by PatternFromBits() in render/player.cpp.
enum class PatternEffect : uint8_t {
  kRings,
  kSpinPlasma,
  kHiphotic,
  kFlame,
  kMetaballs,
  kColoredBursts,
  kGlitter,
  kTheMatrix,
  kThreesine,
  kSolid,     // Including black and warm white.
  kGlow,      // Slow breathing in one color.
  kColoring,  // Any solid RGB color.
  kOther,     // Test and calibration patterns, and the red that unknown reserved patterns show.
};

struct DecodedPattern {
  PatternEffect effect = PatternEffect::kOther;
  // Set for effects that use a palette.
  std::optional<PatternPalette> palette;
  // Set for kSolid, kGlow and kColoring.
  std::optional<WLEDColor> color;
  // The name the JazzLights renderer gives this pattern on a device that isn't all linear, such as "rings-ocean".
  std::string name;
};

// Decodes pattern bits the way PatternFromBits() does on a device that isn't all linear, doesn't skip flame, and isn't
// sound reactive.
DecodedPattern DecodePattern(PatternBits pattern);

// Pattern bits for an effect that uses a palette. Only the bits that select the effect and palette are set, so callers
// should pass the result through RandomizePatternBits() before sending it.
PatternBits PalettePattern(PatternEffect effect, PatternPalette palette);
// Pattern bits for glitter, the-matrix or threesine.
PatternBits PalettelessPattern(PatternEffect effect);
// Pattern bits for a solid RGB color.
PatternBits ColoringPattern(WLEDColor color);

// The JazzLights palette closest to these colors, regardless of their order.
PatternPalette ClosestPalette(const std::array<WLEDColor, 16>& colors);

// Glow if `color` is close enough to one of the glow colors, otherwise coloring with `color`.
PatternBits GlowOrColoringPattern(WLEDColor color);

// How the leader maps each WLED effect. See "Effects" in extras/docs/WLED.md.
enum class WLEDEffectCategory : uint8_t {
  kRings,  // The default, for effects without a closer match.
  kSpinPlasma,
  kHiphotic,
  kFlame,
  kMetaballs,
  kColoredBursts,
  kGlitter,
  kTheMatrix,
  kThreesine,
  kSolid,
  kGlow,
};

const char* WLEDEffectCategoryName(WLEDEffectCategory category);
WLEDEffectCategory CategoryOfWLEDEffect(uint8_t wledEffect);

// The WLED effect IDs that CategoryOfWLEDEffect() maps to something other than rings, named after WLED's FX_MODE_*
// constants. The usermod checks them against WLED's FX.h at compile time.
enum WLEDEffect : uint8_t {
  kWLEDSolid = 0,
  kWLEDBreath = 2,
  kWLEDFade = 12,
  kWLEDTwinkle = 17,
  kWLEDDissolve = 18,
  kWLEDDissolveRandom = 19,
  kWLEDSparkle = 20,
  kWLEDFlashSparkle = 21,
  kWLEDHyperSparkle = 22,
  kWLEDFireworks = 42,
  kWLEDRain = 43,
  kWLEDFireFlicker = 45,
  kWLEDFairyTwinkle = 51,
  kWLEDJuggle = 64,
  kWLEDFire2012 = 66,
  kWLEDFillNoise8 = 69,
  kWLEDNoise1 = 70,
  kWLEDNoise2 = 71,
  kWLEDNoise3 = 72,
  kWLEDNoise4 = 73,
  kWLEDColorTwinkle = 74,
  kWLEDTwinkleFox = 80,
  kWLEDTwinkleCat = 81,
  kWLEDGlitter = 87,
  kWLEDCandle = 88,
  kWLEDStarburst = 89,
  kWLEDExplodingFireworks = 90,
  kWLEDDrip = 96,
  kWLEDPlasma = 97,
  kWLEDHeartbeat = 100,
  kWLEDCandleMulti = 102,
  kWLEDSolidGlitter = 103,
  kWLEDTwinkleUp = 106,
  kWLEDNoisePal = 107,
  kWLEDSineWave = 108,
  kWLEDPhasedNoise = 109,
  kWLED2DPlasmaRotozoom = 114,
  kWLED2DBlobs = 121,
  kWLED2DDistortionWaves = 124,
  kWLED2DSoap = 125,
  kWLEDNoiseFire = 143,
  kWLED2DNoise = 146,
  kWLED2DFireNoise = 149,
  kWLED2DDNA = 152,
  kWLED2DMatrix = 153,
  kWLED2DMetaballs = 154,
  kWLED2DColoredBursts = 167,
  kWLED2DLissajous = 176,
  kWLED2DPlasmaBall = 178,
  kWLED2DHiphotic = 180,
  kWLED2DDNASpiral = 182,
  kWLEDWaveSins = 184,
  kWLEDParticleFire = 188,
  kWLEDParticleFireworks = 189,
  kWLEDParticlePerlin = 191,
  kWLEDParticleBlobs = 201,
  kWLEDPSDrip = 202,
  kWLEDPSStarburst = 211,
  kWLEDPSFire1D = 213,
  kWLEDColorClouds = 218,
};

// WLED palette IDs that have an exact JazzLights equivalent, named after WLED's palette names.
enum WLEDPalette : uint8_t {
  kWLEDPaletteDefault = 0,
  kWLEDPaletteColorsOnly = 5,
  kWLEDPaletteParty = 6,
  kWLEDPaletteCloud = 7,
  kWLEDPaletteLava = 8,
  kWLEDPaletteOcean = 9,
  kWLEDPaletteForest = 10,
  kWLEDPaletteRainbow = 11,
  kWLEDPaletteRainbowBands = 12,
  kWLEDPaletteFire = 35,
};

// What the leader needs to know about the WLED segment it follows.
struct WLEDSegmentState {
  uint8_t effect = kWLEDSolid;
  // Whether WLED shows Solid instead of the effect: 2D effects do that on a 1D segment.
  bool showsSolidFallback = false;
  // Whether the segment's palette is "Default".
  bool paletteIsDefault = true;
  // The palette whose colors the effect shows, see "Palettes" in extras/docs/WLED.md, and its 16 colors.
  uint8_t paletteId = kWLEDPaletteColorsOnly;
  std::array<WLEDColor, 16> palette = {};
  // The segment's first color, with any white channel folded into RGB.
  WLEDColor color;
};

// The pattern a leader advertises for this WLED state, before RandomizePatternBits().
PatternBits WLEDToJazzLights(const WLEDSegmentState& state);

}  // namespace jazzlights

#endif  // JL_WLED_MAPPING_H
