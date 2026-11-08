#include <unity.h>

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "jazzlights/network/manager.h"
#include "jazzlights/protocol/engine.h"
#include "jazzlights/render/player.h"
#include "jazzlights/util/types.h"
#include "jazzlights/wled/mapping.h"
#include "wled_data.h"

// Tests for the WLED usermod's mapping, see extras/docs/WLED.md. wled_data.h comes from
// extras/wled/generate_test_data.py. Run with `pio test -e native -f test_wled -v` to see the mapping tables.

namespace jazzlights {
namespace {

constexpr PatternPalette kAllPalettes[] = {
    PatternPalette::kHeat,  PatternPalette::kLava,   PatternPalette::kOcean,   PatternPalette::kCloud,
    PatternPalette::kParty, PatternPalette::kForest, PatternPalette::kRainbow,
};

constexpr PatternEffect kPaletteEffects[] = {
    PatternEffect::kRings, PatternEffect::kSpinPlasma, PatternEffect::kHiphotic,
    PatternEffect::kFlame, PatternEffect::kMetaballs,  PatternEffect::kColoredBursts,
};

WLEDColor ColorFromHex(uint32_t hex) {
  return WLEDColor{static_cast<uint8_t>(hex >> 16), static_cast<uint8_t>(hex >> 8), static_cast<uint8_t>(hex)};
}

std::array<WLEDColor, 16> ColorsFromHex(const uint32_t (&hex)[16]) {
  std::array<WLEDColor, 16> colors;
  for (size_t i = 0; i < colors.size(); i++) { colors[i] = ColorFromHex(hex[i]); }
  return colors;
}

const char* WLEDEffectNameOf(uint8_t id) {
  for (const WLEDEffectName& effect : kWLEDEffectNames) {
    if (effect.id == id) { return effect.name; }
  }
  return nullptr;
}

// A deterministic pseudorandom sequence, so failures are reproducible.
uint32_t NextTestRandom(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state;
}

}  // namespace

void TestDecodeMatchesRenderer() {
  NetworkManager networkManager;
  Player player(networkManager);
  std::vector<PatternBits> patterns;
  // The rotation, with every palette.
  PatternBits rotation = kStartingPattern;
  for (int i = 0; i < 500; i++) {
    for (uint8_t palette = 0; palette < 8; palette++) { patterns.push_back(ApplyPalette(rotation, palette)); }
    rotation = ComputeNextPattern(rotation);
  }
  // Every reserved type 0 pattern.
  for (uint32_t patternByte = 0; patternByte < 256; patternByte++) { patterns.push_back(patternByte << 8); }
  // Every reserved type, and every subtype of type 3, with every palette and both values of bit 1.
  for (uint32_t reservedType = 1; reservedType < 16; reservedType++) {
    for (uint32_t subtype = 0; subtype < 16; subtype++) {
      for (uint32_t palette = 0; palette < 8; palette++) {
        patterns.push_back((reservedType << 4) | (subtype << 8) | (palette << 13));
        patterns.push_back(0x80000000 | (reservedType << 4) | (subtype << 8) | (palette << 13));
      }
    }
  }
  // Arbitrary bits.
  uint32_t state = 1;
  for (int i = 0; i < 5000; i++) { patterns.push_back(NextTestRandom(&state)); }
  for (PatternBits pattern : patterns) {
    const std::string expected = PatternName(pattern, player);
    const std::string actual = DecodePattern(pattern).name;
    if (expected != actual) {
      char message[128];
      snprintf(message, sizeof(message), "%08x renders as %s but decodes as %s", pattern, expected.c_str(),
               actual.c_str());
      TEST_FAIL_MESSAGE(message);
    }
  }
}

void TestPalettePatterns() {
  uint32_t state = 2;
  for (PatternEffect effect : kPaletteEffects) {
    for (PatternPalette palette : kAllPalettes) {
      const PatternBits pattern = PalettePattern(effect, palette);
      // Randomizing the other bits, as the leader does, keeps the effect and palette.
      for (int i = 0; i < 200; i++) {
        const PatternBits randomized = i == 0 ? pattern : RandomizePatternBits(pattern);
        const DecodedPattern decoded = DecodePattern(randomized);
        TEST_ASSERT(decoded.effect == effect);
        TEST_ASSERT(decoded.palette);
        TEST_ASSERT(*decoded.palette == palette);
      }
      (void)NextTestRandom(&state);
    }
  }
}

void TestPalettelessPatterns() {
  for (PatternEffect effect : {PatternEffect::kGlitter, PatternEffect::kTheMatrix, PatternEffect::kThreesine}) {
    const PatternBits pattern = PalettelessPattern(effect);
    TEST_ASSERT(DecodePattern(pattern).effect == effect);
    TEST_ASSERT_EQUAL_UINT32(pattern, RandomizePatternBits(pattern));
  }
  const WLEDColor color{12, 34, 56};
  const PatternBits coloring = ColoringPattern(color);
  const DecodedPattern decoded = DecodePattern(coloring);
  TEST_ASSERT(decoded.effect == PatternEffect::kColoring);
  TEST_ASSERT(decoded.color);
  TEST_ASSERT(*decoded.color == color);
  TEST_ASSERT_EQUAL_UINT32(coloring, RandomizePatternBits(coloring));
}

void TestWLEDEffectIDs() {
  // The IDs in mapping.h must match WLED's effects. The usermod also checks them against FX.h when it's built.
  const std::pair<uint8_t, const char*> expected[] = {
      {             kWLEDSolid,               "Solid"},
      {            kWLEDBreath,             "Breathe"},
      {              kWLEDFade,                "Fade"},
      {           kWLEDTwinkle,             "Twinkle"},
      {          kWLEDDissolve,            "Dissolve"},
      {    kWLEDDissolveRandom,        "Dissolve Rnd"},
      {           kWLEDSparkle,             "Sparkle"},
      {      kWLEDFlashSparkle,        "Sparkle Dark"},
      {      kWLEDHyperSparkle,            "Sparkle+"},
      {         kWLEDFireworks,           "Fireworks"},
      {              kWLEDRain,                "Rain"},
      {       kWLEDFireFlicker,        "Fire Flicker"},
      {      kWLEDFairyTwinkle,        "Fairytwinkle"},
      {            kWLEDJuggle,              "Juggle"},
      {          kWLEDFire2012,           "Fire 2012"},
      {        kWLEDFillNoise8,          "Fill Noise"},
      {            kWLEDNoise1,             "Noise 1"},
      {            kWLEDNoise2,             "Noise 2"},
      {            kWLEDNoise3,             "Noise 3"},
      {            kWLEDNoise4,             "Noise 4"},
      {      kWLEDColorTwinkle,       "Colortwinkles"},
      {        kWLEDTwinkleFox,          "Twinklefox"},
      {        kWLEDTwinkleCat,          "Twinklecat"},
      {           kWLEDGlitter,             "Glitter"},
      {            kWLEDCandle,              "Candle"},
      {         kWLEDStarburst, "Fireworks Starburst"},
      {kWLEDExplodingFireworks,        "Fireworks 1D"},
      {              kWLEDDrip,                "Drip"},
      {            kWLEDPlasma,              "Plasma"},
      {         kWLEDHeartbeat,           "Heartbeat"},
      {       kWLEDCandleMulti,        "Candle Multi"},
      {      kWLEDSolidGlitter,       "Solid Glitter"},
      {         kWLEDTwinkleUp,           "Twinkleup"},
      {          kWLEDNoisePal,           "Noise Pal"},
      {          kWLEDSineWave,                "Sine"},
      {       kWLEDPhasedNoise,        "Phased Noise"},
      {  kWLED2DPlasmaRotozoom,          "Rotozoomer"},
      {           kWLED2DBlobs,               "Blobs"},
      { kWLED2DDistortionWaves,    "Distortion Waves"},
      {            kWLED2DSoap,                "Soap"},
      {         kWLEDNoiseFire,           "Noisefire"},
      {           kWLED2DNoise,             "Noise2D"},
      {       kWLED2DFireNoise,           "Firenoise"},
      {             kWLED2DDNA,                 "DNA"},
      {          kWLED2DMatrix,              "Matrix"},
      {       kWLED2DMetaballs,           "Metaballs"},
      {   kWLED2DColoredBursts,      "Colored Bursts"},
      {       kWLED2DLissajous,           "Lissajous"},
      {      kWLED2DPlasmaBall,         "Plasma Ball"},
      {        kWLED2DHiphotic,            "Hiphotic"},
      {       kWLED2DDNASpiral,          "DNA Spiral"},
      {          kWLEDWaveSins,            "Wavesins"},
      {      kWLEDParticleFire,             "PS Fire"},
      { kWLEDParticleFireworks,        "PS Fireworks"},
      {    kWLEDParticlePerlin,      "PS Fuzzy Noise"},
      {     kWLEDParticleBlobs,            "PS Blobs"},
      {            kWLEDPSDrip,         "PS DripDrop"},
      {       kWLEDPSStarburst,        "PS Starburst"},
      {          kWLEDPSFire1D,          "PS Fire 1D"},
      {       kWLEDColorClouds,        "Color Clouds"},
  };
  for (const auto& [id, name] : expected) {
    const char* actual = WLEDEffectNameOf(id);
    TEST_ASSERT_NOT_NULL_MESSAGE(actual, name);
    TEST_ASSERT_EQUAL_STRING(name, actual);
  }
  const std::pair<uint8_t, const char*> expectedPalettes[] = {
      {       kWLEDPaletteParty,         "Party"},
      {       kWLEDPaletteCloud,         "Cloud"},
      {        kWLEDPaletteLava,          "Lava"},
      {       kWLEDPaletteOcean,         "Ocean"},
      {      kWLEDPaletteForest,        "Forest"},
      {     kWLEDPaletteRainbow,       "Rainbow"},
      {kWLEDPaletteRainbowBands, "Rainbow Bands"},
      {        kWLEDPaletteFire,          "Fire"},
  };
  for (const auto& [id, name] : expectedPalettes) {
    bool found = false;
    for (const WLEDBuiltInPalette& palette : kWLEDBuiltInPalettes) {
      if (palette.id != id) { continue; }
      TEST_ASSERT_EQUAL_STRING(name, palette.name);
      found = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(found, name);
  }
}

void TestEveryWLEDEffectMaps() {
  printf("\nWLED effect -> JazzLights pattern (with WLED's Ocean palette, on a 2D segment):\n");
  for (const WLEDEffectName& effect : kWLEDEffectNames) {
    WLEDSegmentState state;
    state.effect = effect.id;
    state.paletteIsDefault = false;
    state.paletteId = kWLEDPaletteOcean;
    state.color = WLEDColor{255, 0, 0};
    const DecodedPattern decoded = DecodePattern(WLEDToJazzLights(state));
    printf("  %3u %-22s -> %s\n", effect.id, effect.name, decoded.name.c_str());
    TEST_ASSERT(decoded.effect != PatternEffect::kOther);
  }
  // Effects that the table doesn't know about go to rings.
  WLEDSegmentState unknown;
  unknown.effect = 250;
  unknown.paletteIsDefault = false;
  unknown.paletteId = kWLEDPaletteOcean;
  TEST_ASSERT_EQUAL_STRING("rings-ocean", DecodePattern(WLEDToJazzLights(unknown)).name.c_str());
}

void TestPaletteMapping() {
  // JazzLights' own palettes map to themselves.
  for (PatternPalette palette : kAllPalettes) { TEST_ASSERT(ClosestPalette(PatternPaletteColors(palette)) == palette); }
  printf("\nWLED palette -> JazzLights palette:\n");
  for (const WLEDBuiltInPalette& builtIn : kWLEDBuiltInPalettes) {
    WLEDSegmentState state;
    state.effect = 0xFF;  // Unknown, so rings.
    state.paletteIsDefault = false;
    state.paletteId = builtIn.id;
    state.palette = ColorsFromHex(builtIn.colors);
    const DecodedPattern decoded = DecodePattern(WLEDToJazzLights(state));
    TEST_ASSERT(decoded.palette);
    printf("  %3u %-16s -> %s\n", builtIn.id, builtIn.name, PatternPaletteName(*decoded.palette));
  }
  // Exact equivalents don't depend on the colors.
  const std::pair<uint8_t, PatternPalette> exact[] = {
      {       kWLEDPaletteParty,   PatternPalette::kParty},
      {       kWLEDPaletteCloud,   PatternPalette::kCloud},
      {        kWLEDPaletteLava,    PatternPalette::kLava},
      {       kWLEDPaletteOcean,   PatternPalette::kOcean},
      {      kWLEDPaletteForest,  PatternPalette::kForest},
      {     kWLEDPaletteRainbow, PatternPalette::kRainbow},
      {kWLEDPaletteRainbowBands, PatternPalette::kRainbow},
      {        kWLEDPaletteFire,    PatternPalette::kHeat},
  };
  for (const auto& [id, palette] : exact) {
    WLEDSegmentState state;
    state.effect = 0xFF;
    state.paletteIsDefault = false;
    state.paletteId = id;
    TEST_ASSERT(*DecodePattern(WLEDToJazzLights(state)).palette == palette);
  }
}

void TestGlowColors() {
  const std::pair<uint32_t, const char*> expected[] = {
      {0xFF0000,    "glow-red"},
      {0x400000,    "glow-red"},
      {0x00FF00,  "glow-green"},
      {0x0000FF,   "glow-blue"},
      {0xFF00FF, "glow-purple"},
      {0x00FFFF,   "glow-cyan"},
      {0xFFFF00, "glow-yellow"},
      {0xFFFFFF,  "glow-white"},
      {0xFF4305,   "glow-warm"},
      {0x000000,    "coloring"},
      {0xFF0080,    "coloring"},
  };
  for (const auto& [hex, name] : expected) {
    TEST_ASSERT_EQUAL_STRING(name, DecodePattern(GlowOrColoringPattern(ColorFromHex(hex))).name.c_str());
  }
  printf("\nBreathe color -> JazzLights pattern:\n");
  for (uint32_t hex : {0xFF8000u, 0xFFC000u, 0x80FF00u, 0x00FF80u, 0x0080FFu, 0x8000FFu, 0xFF0080u, 0xFFE0C0u}) {
    printf("  #%06X -> %s\n", hex, DecodePattern(GlowOrColoringPattern(ColorFromHex(hex))).name.c_str());
  }
}

void TestSpecialCases() {
  WLEDSegmentState state;
  state.color = WLEDColor{10, 20, 30};
  state.paletteIsDefault = false;
  state.paletteId = kWLEDPaletteOcean;

  // Solid sends the exact color.
  state.effect = kWLEDSolid;
  TEST_ASSERT_EQUAL_UINT32(ColoringPattern(state.color), WLEDToJazzLights(state));

  // A 2D effect on a 1D segment shows Solid.
  state.effect = kWLED2DHiphotic;
  TEST_ASSERT_EQUAL_STRING("hiphotic-ocean", DecodePattern(WLEDToJazzLights(state)).name.c_str());
  state.showsSolidFallback = true;
  TEST_ASSERT_EQUAL_UINT32(ColoringPattern(state.color), WLEDToJazzLights(state));
  state.showsSolidFallback = false;

  // Fires use heat when the palette is Default, and the palette otherwise.
  state.effect = kWLEDFireFlicker;
  TEST_ASSERT_EQUAL_STRING("flame-ocean", DecodePattern(WLEDToJazzLights(state)).name.c_str());
  state.paletteIsDefault = true;
  TEST_ASSERT_EQUAL_STRING("flame-heat", DecodePattern(WLEDToJazzLights(state)).name.c_str());
}

int RunUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(TestDecodeMatchesRenderer);
  RUN_TEST(TestPalettePatterns);
  RUN_TEST(TestPalettelessPatterns);
  RUN_TEST(TestWLEDEffectIDs);
  RUN_TEST(TestEveryWLEDEffectMaps);
  RUN_TEST(TestPaletteMapping);
  RUN_TEST(TestGlowColors);
  RUN_TEST(TestSpecialCases);
  return UNITY_END();
}

}  // namespace jazzlights

void setUp() {}

void tearDown() {}

int main(int /*argc*/, char** /*argv*/) { return jazzlights::RunUnityTests(); }
