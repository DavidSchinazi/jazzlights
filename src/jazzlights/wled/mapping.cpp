#include "jazzlights/wled/mapping.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace jazzlights {
namespace {

constexpr WLEDColor ColorFromHex(uint32_t hex) {
  return WLEDColor{static_cast<uint8_t>(hex >> 16), static_cast<uint8_t>(hex >> 8), static_cast<uint8_t>(hex)};
}

// FastLED's stock palettes, which JazzLights renders with. These are copied here because FastLED isn't available in the
// WLED usermod, see FastLED's colorpalettes.cpp.
// clang-format off
constexpr std::array<WLEDColor, 16> kHeatColors = {
    ColorFromHex(0x000000), ColorFromHex(0x330000), ColorFromHex(0x660000), ColorFromHex(0x990000),
    ColorFromHex(0xCC0000), ColorFromHex(0xFF0000), ColorFromHex(0xFF3300), ColorFromHex(0xFF6600),
    ColorFromHex(0xFF9900), ColorFromHex(0xFFCC00), ColorFromHex(0xFFFF00), ColorFromHex(0xFFFF33),
    ColorFromHex(0xFFFF66), ColorFromHex(0xFFFF99), ColorFromHex(0xFFFFCC), ColorFromHex(0xFFFFFF)};
constexpr std::array<WLEDColor, 16> kLavaColors = {
    ColorFromHex(0x000000), ColorFromHex(0x800000), ColorFromHex(0x000000), ColorFromHex(0x800000),
    ColorFromHex(0x8B0000), ColorFromHex(0x8B0000), ColorFromHex(0x800000), ColorFromHex(0x8B0000),
    ColorFromHex(0x8B0000), ColorFromHex(0x8B0000), ColorFromHex(0xFF0000), ColorFromHex(0xFFA500),
    ColorFromHex(0xFFFFFF), ColorFromHex(0xFFA500), ColorFromHex(0xFF0000), ColorFromHex(0x8B0000)};
constexpr std::array<WLEDColor, 16> kOceanColors = {
    ColorFromHex(0x191970), ColorFromHex(0x00008B), ColorFromHex(0x191970), ColorFromHex(0x000080),
    ColorFromHex(0x00008B), ColorFromHex(0x0000CD), ColorFromHex(0x2E8B57), ColorFromHex(0x008080),
    ColorFromHex(0x5F9EA0), ColorFromHex(0x0000FF), ColorFromHex(0x008B8B), ColorFromHex(0x6495ED),
    ColorFromHex(0x7FFFD4), ColorFromHex(0x2E8B57), ColorFromHex(0x00FFFF), ColorFromHex(0x87CEFA)};
constexpr std::array<WLEDColor, 16> kCloudColors = {
    ColorFromHex(0x0000FF), ColorFromHex(0x00008B), ColorFromHex(0x00008B), ColorFromHex(0x00008B),
    ColorFromHex(0x00008B), ColorFromHex(0x00008B), ColorFromHex(0x00008B), ColorFromHex(0x00008B),
    ColorFromHex(0x0000FF), ColorFromHex(0x00008B), ColorFromHex(0x87CEEB), ColorFromHex(0x87CEEB),
    ColorFromHex(0xADD8E6), ColorFromHex(0xFFFFFF), ColorFromHex(0xADD8E6), ColorFromHex(0x87CEEB)};
constexpr std::array<WLEDColor, 16> kPartyColors = {
    ColorFromHex(0x5500AB), ColorFromHex(0x84007C), ColorFromHex(0xB5004B), ColorFromHex(0xE5001B),
    ColorFromHex(0xE81700), ColorFromHex(0xB84700), ColorFromHex(0xAB7700), ColorFromHex(0xABAB00),
    ColorFromHex(0xAB5500), ColorFromHex(0xDD2200), ColorFromHex(0xF2000E), ColorFromHex(0xC2003E),
    ColorFromHex(0x8F0071), ColorFromHex(0x5F00A1), ColorFromHex(0x2F00D0), ColorFromHex(0x0007F9)};
constexpr std::array<WLEDColor, 16> kForestColors = {
    ColorFromHex(0x006400), ColorFromHex(0x006400), ColorFromHex(0x556B2F), ColorFromHex(0x006400),
    ColorFromHex(0x008000), ColorFromHex(0x228B22), ColorFromHex(0x6B8E23), ColorFromHex(0x008000),
    ColorFromHex(0x2E8B57), ColorFromHex(0x66CDAA), ColorFromHex(0x32CD32), ColorFromHex(0x9ACD32),
    ColorFromHex(0x90EE90), ColorFromHex(0x7CFC00), ColorFromHex(0x66CDAA), ColorFromHex(0x228B22)};
constexpr std::array<WLEDColor, 16> kRainbowColors = {
    ColorFromHex(0xFF0000), ColorFromHex(0xD52A00), ColorFromHex(0xAB5500), ColorFromHex(0xAB7F00),
    ColorFromHex(0xABAB00), ColorFromHex(0x56D500), ColorFromHex(0x00FF00), ColorFromHex(0x00D52A),
    ColorFromHex(0x00AB55), ColorFromHex(0x0056AA), ColorFromHex(0x0000FF), ColorFromHex(0x2A00D5),
    ColorFromHex(0x5500AB), ColorFromHex(0x7F0081), ColorFromHex(0xAB0055), ColorFromHex(0xD5002B)};
// clang-format on

}  // namespace

const std::array<WLEDColor, 16>& PatternPaletteColors(PatternPalette palette) {
  switch (palette) {
    case PatternPalette::kHeat: return kHeatColors;
    case PatternPalette::kLava: return kLavaColors;
    case PatternPalette::kOcean: return kOceanColors;
    case PatternPalette::kCloud: return kCloudColors;
    case PatternPalette::kParty: return kPartyColors;
    case PatternPalette::kForest: return kForestColors;
    case PatternPalette::kRainbow: return kRainbowColors;
  }
  return kRainbowColors;
}

namespace {

// The colors of the glow patterns, which are reserved patterns of type 0, in the order of their bytes 0x08 to 0x0E,
// then 0x16. These match the colors in PatternFromBits().
struct GlowColor {
  WLEDColor color;
  uint8_t patternByte;
  const char* name;
};
constexpr GlowColor kGlowColors[] = {
    {ColorFromHex(0xFF0000), 0x08,    "red"},
    {ColorFromHex(0x008000), 0x09,  "green"},
    {ColorFromHex(0x0000FF), 0x0A,   "blue"},
    {ColorFromHex(0x800080), 0x0B, "purple"},
    {ColorFromHex(0x00FFFF), 0x0C,   "cyan"},
    {ColorFromHex(0xFFFF00), 0x0D, "yellow"},
    {ColorFromHex(0xFFFFFF), 0x0E,  "white"},
    {ColorFromHex(0xFF4305), 0x16,   "warm"},
};
// The solid colors, which are reserved patterns of type 0.
constexpr GlowColor kSolidColors[] = {
    {ColorFromHex(0x000000), 0x00,  "black"},
    {ColorFromHex(0xFF0000), 0x01,    "red"},
    {ColorFromHex(0x008000), 0x02,  "green"},
    {ColorFromHex(0x0000FF), 0x03,   "blue"},
    {ColorFromHex(0x800080), 0x04, "purple"},
    {ColorFromHex(0x00FFFF), 0x05,   "cyan"},
    {ColorFromHex(0xFFFF00), 0x06, "yellow"},
    {ColorFromHex(0xFFFFFF), 0x07,  "white"},
    {ColorFromHex(0xFF4305), 0x15,   "warm"},
};

// How far a color can be from the closest glow color, in OKLab after scaling both to full brightness, for a glow to
// stand in for it. Glow only has eight colors, so beyond this we send the exact color without breathing instead.
constexpr float kMaxGlowColorDistance = 0.1f;

struct Oklab {
  float l;
  float a;
  float b;
};

float SrgbToLinear(uint8_t value) {
  const float x = value / 255.0f;
  return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}

// See https://bottosson.github.io/posts/oklab/
Oklab ToOklab(WLEDColor color) {
  const float r = SrgbToLinear(color.r);
  const float g = SrgbToLinear(color.g);
  const float b = SrgbToLinear(color.b);
  const float l = std::cbrt(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
  const float m = std::cbrt(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
  const float s = std::cbrt(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
  return Oklab{0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s,
               1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
               0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s};
}

float Distance(const Oklab& x, const Oklab& y) {
  const float dl = x.l - y.l;
  const float da = x.a - y.a;
  const float db = x.b - y.b;
  return std::sqrt(dl * dl + da * da + db * db);
}

// Scales a color so that its brightest channel is 255, to compare hues regardless of brightness.
WLEDColor AtFullBrightness(WLEDColor color) {
  const uint8_t maxChannel = std::max({color.r, color.g, color.b});
  if (maxChannel == 0) { return color; }
  auto scale = [maxChannel](uint8_t value) {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * 255 + maxChannel / 2) / maxChannel);
  };
  return WLEDColor{scale(color.r), scale(color.g), scale(color.b)};
}

constexpr float kPi = 3.14159265358979f;

// A palette's hues, weighted by how saturated they are: 12 bins of 30 degrees of OKLab hue, then one bin for black,
// white and greys. Each color adds up to 1 in total, so the bins add up to the number of colors.
constexpr size_t kNumHueBins = 12;
using HueHistogram = std::array<float, kNumHueBins + 1>;
// OKLab chroma from which a color counts fully as a hue rather than as a grey.
constexpr float kFullChroma = 0.1f;

HueHistogram HueHistogramOf(const std::array<WLEDColor, 16>& colors) {
  HueHistogram histogram = {};
  for (const WLEDColor& color : colors) {
    const Oklab lab = ToOklab(color);
    const float chroma = std::hypot(lab.a, lab.b);
    const float weight = std::min(chroma / kFullChroma, 1.0f);
    float hue = std::atan2(lab.b, lab.a) * static_cast<float>(kNumHueBins) / (2.0f * kPi);
    if (hue < 0) { hue += kNumHueBins; }
    // Split the weight between the two closest bins.
    const size_t bin = static_cast<size_t>(hue) % kNumHueBins;
    const float fraction = hue - std::floor(hue);
    histogram[bin] += weight * (1.0f - fraction);
    histogram[(bin + 1) % kNumHueBins] += weight * fraction;
    histogram[kNumHueBins] += 1.0f - weight;
  }
  // Blur the hues a little, so that close hues in neighboring bins still match.
  HueHistogram blurred = histogram;
  for (size_t bin = 0; bin < kNumHueBins; bin++) {
    blurred[bin] = 0.25f * histogram[(bin + kNumHueBins - 1) % kNumHueBins] + 0.5f * histogram[bin] +
                   0.25f * histogram[(bin + 1) % kNumHueBins];
  }
  return blurred;
}

float HistogramDistance(const HueHistogram& x, const HueHistogram& y) {
  float distance = 0;
  for (size_t bin = 0; bin < x.size(); bin++) { distance += std::abs(x[bin] - y[bin]); }
  return distance;
}

std::optional<PatternPalette> ExactPalette(uint8_t wledPalette) {
  switch (wledPalette) {
    case kWLEDPaletteParty: return PatternPalette::kParty;
    case kWLEDPaletteCloud: return PatternPalette::kCloud;
    case kWLEDPaletteLava: return PatternPalette::kLava;
    case kWLEDPaletteOcean: return PatternPalette::kOcean;
    case kWLEDPaletteForest: return PatternPalette::kForest;
    case kWLEDPaletteRainbow:
    case kWLEDPaletteRainbowBands: return PatternPalette::kRainbow;
    case kWLEDPaletteFire: return PatternPalette::kHeat;
  }
  return std::nullopt;
}

PatternBits WithPalette(PatternBits pattern, PatternPalette palette) {
  return pattern | (static_cast<PatternBits>(palette) << 13);
}

std::string NameWithPalette(const char* prefix, PatternPalette palette) {
  return std::string(prefix) + "-" + PatternPaletteName(palette);
}

DecodedPattern Decoded(PatternEffect effect, std::string name) {
  DecodedPattern decoded;
  decoded.effect = effect;
  decoded.name = std::move(name);
  return decoded;
}

DecodedPattern DecodedWithPalette(PatternEffect effect, const char* prefix, PatternPalette palette) {
  DecodedPattern decoded = Decoded(effect, NameWithPalette(prefix, palette));
  decoded.palette = palette;
  return decoded;
}

DecodedPattern DecodedWithColor(PatternEffect effect, std::string name, WLEDColor color) {
  DecodedPattern decoded = Decoded(effect, std::move(name));
  decoded.color = color;
  return decoded;
}

}  // namespace

const char* PatternPaletteName(PatternPalette palette) {
  switch (palette) {
    case PatternPalette::kHeat: return "heat";
    case PatternPalette::kLava: return "lava";
    case PatternPalette::kOcean: return "ocean";
    case PatternPalette::kCloud: return "cloud";
    case PatternPalette::kParty: return "party";
    case PatternPalette::kForest: return "forest";
    case PatternPalette::kRainbow: return "rainbow";
  }
  return "unknown";
}

DecodedPattern DecodePattern(PatternBits pattern) {
  // Palette bits 17-19, like PaletteFromPattern(). Values 6 and 7 are both rainbow.
  const uint8_t paletteBits = (pattern >> 13) & 0x7;
  const PatternPalette palette = paletteBits >= 6 ? PatternPalette::kRainbow : static_cast<PatternPalette>(paletteBits);
  const bool bit1 = (pattern & 0x80000000) != 0;
  const bool bit2 = (pattern & 0x40000000) != 0;
  if ((pattern & 0xF) != 0) {
    if (bit1) {
      if (bit2) { return DecodedWithPalette(PatternEffect::kSpinPlasma, "sp", palette); }
      return DecodedWithPalette(PatternEffect::kHiphotic, "hiphotic", palette);
    }
    if (bit2) { return DecodedWithPalette(PatternEffect::kFlame, "flame", palette); }
    return DecodedWithPalette(PatternEffect::kRings, "rings", palette);
  }
  const uint8_t reservedType = (pattern >> 4) & 0xF;
  if (reservedType == 0x0) {
    const uint8_t patternByte = (pattern >> 8) & 0xFF;
    for (const GlowColor& solid : kSolidColors) {
      if (solid.patternByte == patternByte) { return DecodedWithColor(PatternEffect::kSolid, solid.name, solid.color); }
    }
    for (const GlowColor& glow : kGlowColors) {
      if (glow.patternByte == patternByte) {
        return DecodedWithColor(PatternEffect::kGlow, std::string("glow-") + glow.name, glow.color);
      }
    }
    switch (patternByte) {
      case 0x0F: return Decoded(PatternEffect::kOther, "synctest");
      case 0x10: return Decoded(PatternEffect::kOther, "calibration");
      case 0x11: return Decoded(PatternEffect::kOther, "follow-strand");
      case 0x12: return Decoded(PatternEffect::kGlitter, "glitter");
      case 0x13: return Decoded(PatternEffect::kTheMatrix, "the-matrix");
      case 0x14: return Decoded(PatternEffect::kThreesine, "threesine");
      // 0xFF is the creatures' pattern, which other devices show as white glow.
      case 0xFF: return DecodedWithColor(PatternEffect::kGlow, "glow-white", ColorFromHex(0xFFFFFF));
    }
  } else if (reservedType == 0x1) {
    return Decoded(PatternEffect::kOther, "mapping");
  } else if (reservedType == 0x2) {
    return DecodedWithColor(PatternEffect::kColoring, "coloring", ColorFromHex(pattern >> 8));
  } else if (reservedType == 0x3 && ((pattern >> 8) & 0xF) == 0x0) {
    if (bit1) { return DecodedWithPalette(PatternEffect::kMetaballs, "metaballs", palette); }
    return DecodedWithPalette(PatternEffect::kColoredBursts, "bursts", palette);
  }
  // PatternFromBits() shows red for every other reserved pattern.
  return DecodedWithColor(PatternEffect::kOther, "red", ColorFromHex(0xFF0000));
}

PatternBits PalettePattern(PatternEffect effect, PatternPalette palette) {
  switch (effect) {
    case PatternEffect::kRings: return WithPalette(0x00000001, palette);
    case PatternEffect::kFlame: return WithPalette(0x40000001, palette);
    case PatternEffect::kHiphotic: return WithPalette(0x80000001, palette);
    case PatternEffect::kSpinPlasma: return WithPalette(0xC0000001, palette);
    case PatternEffect::kMetaballs: return WithPalette(0x80000030, palette);
    case PatternEffect::kColoredBursts: return WithPalette(0x00000030, palette);
    default: break;
  }
  return WithPalette(0x00000001, palette);
}

PatternBits PalettelessPattern(PatternEffect effect) {
  switch (effect) {
    case PatternEffect::kGlitter: return 0x00001200;
    case PatternEffect::kTheMatrix: return 0x00001300;
    case PatternEffect::kThreesine: return 0x00001400;
    default: break;
  }
  return 0x00001200;
}

PatternBits ColoringPattern(WLEDColor color) {
  return (static_cast<PatternBits>(color.r) << 24) | (static_cast<PatternBits>(color.g) << 16) |
         (static_cast<PatternBits>(color.b) << 8) | 0x20;
}

PatternPalette ClosestPalette(const std::array<WLEDColor, 16>& colors) {
  // Compare how much of each hue the palettes have. Palettes are mostly about their hues: comparing the colors
  // themselves let lightness dominate, which made heat, with its range from black to white, the closest palette to
  // many unrelated ones.
  const HueHistogram histogram = HueHistogramOf(colors);
  PatternPalette closest = PatternPalette::kRainbow;
  float closestDistance = std::numeric_limits<float>::max();
  for (uint8_t i = 0; i < kNumPatternPalettes; i++) {
    const PatternPalette palette = static_cast<PatternPalette>(i);
    const float distance = HistogramDistance(histogram, HueHistogramOf(PatternPaletteColors(palette)));
    if (distance < closestDistance) {
      closestDistance = distance;
      closest = palette;
    }
  }
  return closest;
}

PatternBits GlowOrColoringPattern(WLEDColor color) {
  const Oklab colorLab = ToOklab(AtFullBrightness(color));
  const GlowColor* closest = nullptr;
  float closestDistance = std::numeric_limits<float>::max();
  for (const GlowColor& glow : kGlowColors) {
    const float distance = Distance(colorLab, ToOklab(AtFullBrightness(glow.color)));
    if (distance < closestDistance) {
      closestDistance = distance;
      closest = &glow;
    }
  }
  if (closest == nullptr || closestDistance > kMaxGlowColorDistance || color == WLEDColor{}) {
    return ColoringPattern(color);
  }
  return static_cast<PatternBits>(closest->patternByte) << 8;
}

const char* WLEDEffectCategoryName(WLEDEffectCategory category) {
  switch (category) {
    case WLEDEffectCategory::kRings: return "rings";
    case WLEDEffectCategory::kSpinPlasma: return "sp";
    case WLEDEffectCategory::kHiphotic: return "hiphotic";
    case WLEDEffectCategory::kFlame: return "flame";
    case WLEDEffectCategory::kMetaballs: return "metaballs";
    case WLEDEffectCategory::kColoredBursts: return "bursts";
    case WLEDEffectCategory::kGlitter: return "glitter";
    case WLEDEffectCategory::kTheMatrix: return "the-matrix";
    case WLEDEffectCategory::kThreesine: return "threesine";
    case WLEDEffectCategory::kSolid: return "coloring";
    case WLEDEffectCategory::kGlow: return "glow";
  }
  return "unknown";
}

WLEDEffectCategory CategoryOfWLEDEffect(uint8_t wledEffect) {
  switch (wledEffect) {
    // The same effect.
    case kWLED2DHiphotic: return WLEDEffectCategory::kHiphotic;
    case kWLED2DMetaballs: return WLEDEffectCategory::kMetaballs;
    case kWLED2DColoredBursts: return WLEDEffectCategory::kColoredBursts;
    case kWLED2DMatrix: return WLEDEffectCategory::kTheMatrix;
    case kWLEDGlitter: return WLEDEffectCategory::kGlitter;
    case kWLEDFire2012: return WLEDEffectCategory::kFlame;

    // The same look.
    case kWLEDSolid: return WLEDEffectCategory::kSolid;

    case kWLEDBreath:
    case kWLEDFade:
    case kWLEDHeartbeat: return WLEDEffectCategory::kGlow;

    // Plasmas and noise fields.
    case kWLEDPlasma:
    case kWLED2DPlasmaRotozoom:
    case kWLED2DPlasmaBall:
    case kWLEDFillNoise8:
    case kWLEDNoise1:
    case kWLEDNoise2:
    case kWLEDNoise3:
    case kWLEDNoise4:
    case kWLEDNoisePal:
    case kWLEDPhasedNoise:
    case kWLED2DNoise:
    case kWLED2DSoap:
    case kWLED2DDistortionWaves:
    case kWLEDParticlePerlin:
    case kWLEDColorClouds: return WLEDEffectCategory::kSpinPlasma;

    // Fires.
    case kWLEDFireFlicker:
    case kWLEDCandle:
    case kWLEDCandleMulti:
    case kWLED2DFireNoise:
    case kWLEDNoiseFire:
    case kWLEDParticleFire:
    case kWLEDPSFire1D: return WLEDEffectCategory::kFlame;

    // Blobs.
    case kWLED2DBlobs:
    case kWLEDParticleBlobs: return WLEDEffectCategory::kMetaballs;

    // Bursts and fireworks.
    case kWLEDFireworks:
    case kWLEDStarburst:
    case kWLEDExplodingFireworks:
    case kWLEDParticleFireworks:
    case kWLEDPSStarburst:
    case kWLEDJuggle: return WLEDEffectCategory::kColoredBursts;

    // Falling drops.
    case kWLEDRain:
    case kWLEDDrip:
    case kWLEDPSDrip: return WLEDEffectCategory::kTheMatrix;

    // Sparkles and twinkles.
    case kWLEDSolidGlitter:
    case kWLEDSparkle:
    case kWLEDFlashSparkle:
    case kWLEDHyperSparkle:
    case kWLEDTwinkle:
    case kWLEDTwinkleFox:
    case kWLEDTwinkleCat:
    case kWLEDColorTwinkle:
    case kWLEDDissolve:
    case kWLEDDissolveRandom:
    case kWLEDFairyTwinkle:
    case kWLEDTwinkleUp: return WLEDEffectCategory::kGlitter;

    // Sine waves.
    case kWLEDSineWave:
    case kWLEDWaveSins:
    case kWLED2DDNA:
    case kWLED2DDNASpiral:
    case kWLED2DLissajous: return WLEDEffectCategory::kThreesine;
  }
  // Everything else, including palettes scrolling across the device, chases, strobes, wipes, audio-reactive effects,
  // and effects this table doesn't know about.
  return WLEDEffectCategory::kRings;
}

namespace {

uint8_t WLEDPaletteFor(PatternPalette palette) {
  switch (palette) {
    case PatternPalette::kHeat: return kWLEDPaletteFire;
    case PatternPalette::kLava: return kWLEDPaletteLava;
    case PatternPalette::kOcean: return kWLEDPaletteOcean;
    case PatternPalette::kCloud: return kWLEDPaletteCloud;
    case PatternPalette::kParty: return kWLEDPaletteParty;
    case PatternPalette::kForest: return kWLEDPaletteForest;
    case PatternPalette::kRainbow: return kWLEDPaletteRainbow;
  }
  return kWLEDPaletteRainbow;
}

WLEDTarget Target(uint8_t effect, uint8_t palette) {
  WLEDTarget target;
  target.effect = effect;
  target.palette = palette;
  return target;
}

}  // namespace

std::optional<WLEDTarget> JazzLightsToWLED(PatternBits pattern, bool segmentIs2D) {
  const DecodedPattern decoded = DecodePattern(pattern);
  const uint8_t palette = decoded.palette ? WLEDPaletteFor(*decoded.palette) : kWLEDPaletteDefault;
  switch (decoded.effect) {
    case PatternEffect::kRings:
      // WLED's Rainbow effect scrolls a rainbow across the segment, and its Palette effect scrolls a palette.
      if (decoded.palette == PatternPalette::kRainbow) { return Target(kWLEDRainbowCycle, palette); }
      return Target(kWLEDPalette, palette);
    case PatternEffect::kSpinPlasma: return Target(segmentIs2D ? kWLED2DPlasmaRotozoom : kWLEDPlasma, palette);
    case PatternEffect::kHiphotic: return Target(segmentIs2D ? kWLED2DHiphotic : kWLEDNoisePal, palette);
    case PatternEffect::kFlame: return Target(kWLEDFire2012, palette);
    case PatternEffect::kMetaballs: return Target(segmentIs2D ? kWLED2DMetaballs : kWLEDPlasma, palette);
    case PatternEffect::kColoredBursts: return Target(segmentIs2D ? kWLED2DColoredBursts : kWLEDJuggle, palette);
    case PatternEffect::kGlitter: return Target(kWLEDTwinkleFox, kWLEDPaletteRainbow);
    case PatternEffect::kTheMatrix: return Target(segmentIs2D ? kWLED2DMatrix : kWLEDRain, kWLEDPaletteDefault);
    case PatternEffect::kThreesine: return Target(kWLEDSineWave, kWLEDPaletteRainbow);
    case PatternEffect::kSolid:
    case PatternEffect::kColoring: {
      WLEDTarget target = Target(kWLEDSolid, kWLEDPaletteDefault);
      target.color = decoded.color;
      return target;
    }
    case PatternEffect::kGlow: {
      WLEDTarget target = Target(kWLEDBreath, kWLEDPaletteDefault);
      target.color = decoded.color;
      target.blackBackground = true;
      return target;
    }
    case PatternEffect::kOther: break;
  }
  return std::nullopt;
}

PatternBits WLEDToJazzLights(const WLEDSegmentState& state) {
  if (state.showsSolidFallback) { return ColoringPattern(state.color); }
  const std::optional<PatternPalette> exactPalette = ExactPalette(state.paletteId);
  const PatternPalette palette = exactPalette ? *exactPalette : ClosestPalette(state.palette);
  switch (CategoryOfWLEDEffect(state.effect)) {
    case WLEDEffectCategory::kRings: return PalettePattern(PatternEffect::kRings, palette);
    case WLEDEffectCategory::kSpinPlasma: return PalettePattern(PatternEffect::kSpinPlasma, palette);
    case WLEDEffectCategory::kHiphotic: return PalettePattern(PatternEffect::kHiphotic, palette);
    case WLEDEffectCategory::kFlame:
      return PalettePattern(PatternEffect::kFlame, state.paletteIsDefault ? PatternPalette::kHeat : palette);
    case WLEDEffectCategory::kMetaballs: return PalettePattern(PatternEffect::kMetaballs, palette);
    case WLEDEffectCategory::kColoredBursts: return PalettePattern(PatternEffect::kColoredBursts, palette);
    case WLEDEffectCategory::kGlitter: return PalettelessPattern(PatternEffect::kGlitter);
    case WLEDEffectCategory::kTheMatrix: return PalettelessPattern(PatternEffect::kTheMatrix);
    case WLEDEffectCategory::kThreesine: return PalettelessPattern(PatternEffect::kThreesine);
    case WLEDEffectCategory::kSolid: return ColoringPattern(state.color);
    case WLEDEffectCategory::kGlow: return GlowOrColoringPattern(state.color);
  }
  return PalettePattern(PatternEffect::kRings, palette);
}

}  // namespace jazzlights
