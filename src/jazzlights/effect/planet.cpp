#include "jazzlights/effect/planet.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include "jazzlights/util/log.h"

namespace jazzlights {

namespace {

const TProgmemRGBPalette16* GetPlanetPalette(Planet planet) {
  switch (planet) {
    case Planet::Mercury: {
      static const TProgmemRGBPalette16 MercuryPalette_p FL_PROGMEM = {
          CRGB::Black, CRGB::DimGray, CRGB::Gray, CRGB::White, CRGB::SlateGray, CRGB::DarkGray,
          CRGB::Black, CRGB::DimGray, CRGB::Gray, CRGB::White, CRGB::SlateGray, CRGB::DarkGray,
          CRGB::Black, CRGB::DimGray, CRGB::Gray, CRGB::White};
      return &MercuryPalette_p;
    }
    case Planet::Venus: {
      static const TProgmemRGBPalette16 VenusPalette_p FL_PROGMEM = {
          CRGB::Yellow, CRGB::Orange, CRGB::LightYellow, CRGB::Gold, CRGB::Beige,       CRGB::Khaki,
          CRGB::Orange, CRGB::White,  CRGB::Yellow,      CRGB::Gold, CRGB::LightYellow, CRGB::Khaki,
          CRGB::Beige,  CRGB::Orange, CRGB::Yellow,      CRGB::White};
      return &VenusPalette_p;
    }
    case Planet::Earth: {
      static const TProgmemRGBPalette16 EarthPalette_p FL_PROGMEM = {
          CRGB::Blue, CRGB::Green, CRGB::White, CRGB::DarkBlue,  CRGB::Blue, CRGB::Green, CRGB::White, CRGB::DarkGreen,
          CRGB::Blue, CRGB::Green, CRGB::White, CRGB::LightBlue, CRGB::Blue, CRGB::Green, CRGB::White, CRGB::SeaGreen};
      return &EarthPalette_p;
    }
    case Planet::Mars: {
      static const TProgmemRGBPalette16 MarsPalette_p FL_PROGMEM = {
          CRGB::Red,   CRGB::Black,   CRGB::DarkRed,   CRGB::OrangeRed, CRGB::Maroon, CRGB::Red,
          CRGB::Black, CRGB::White,   CRGB::DarkRed,   CRGB::OrangeRed, CRGB::Maroon, CRGB::Red,
          CRGB::Black, CRGB::DarkRed, CRGB::OrangeRed, CRGB::White};
      return &MarsPalette_p;
    }
    case Planet::Jupiter: {
      static const TProgmemRGBPalette16 JupiterPalette_p FL_PROGMEM = {
          CRGB::Sienna, CRGB::BurlyWood, CRGB::SandyBrown, CRGB::White,     CRGB::Sienna,     CRGB::Orange,
          CRGB::Maroon, CRGB::Tan,       CRGB::Sienna,     CRGB::BurlyWood, CRGB::SandyBrown, CRGB::White,
          CRGB::Sienna, CRGB::Orange,    CRGB::Maroon,     CRGB::Tan};
      return &JupiterPalette_p;
    }
    case Planet::Saturn: {
      static const TProgmemRGBPalette16 SaturnPalette_p FL_PROGMEM = {
          CRGB::Wheat,    CRGB::NavajoWhite, CRGB::Sienna,   CRGB::Gold,        CRGB::Wheat,  CRGB::Tan,
          CRGB::Moccasin, CRGB::White,       CRGB::Wheat,    CRGB::NavajoWhite, CRGB::Sienna, CRGB::Gold,
          CRGB::Wheat,    CRGB::Tan,         CRGB::Moccasin, CRGB::White};
      return &SaturnPalette_p;
    }
    case Planet::Uranus: {
      static const TProgmemRGBPalette16 UranusPalette_p FL_PROGMEM = {
          CRGB::LightCyan, CRGB::PaleTurquoise, CRGB::CadetBlue,  CRGB::Turquoise,
          CRGB::LightCyan, CRGB::PaleTurquoise, CRGB::Aquamarine, CRGB::White,
          CRGB::LightCyan, CRGB::PaleTurquoise, CRGB::CadetBlue,  CRGB::Turquoise,
          CRGB::LightCyan, CRGB::PaleTurquoise, CRGB::Aquamarine, CRGB::White};
      return &UranusPalette_p;
    }
    case Planet::Neptune: {
      static const TProgmemRGBPalette16 NeptunePalette_p FL_PROGMEM = {
          CRGB::Blue,      CRGB::DeepSkyBlue, CRGB::Navy,      CRGB::MidnightBlue, CRGB::Blue, CRGB::DeepSkyBlue,
          CRGB::RoyalBlue, CRGB::White,       CRGB::Blue,      CRGB::DeepSkyBlue,  CRGB::Navy, CRGB::MidnightBlue,
          CRGB::Blue,      CRGB::DeepSkyBlue, CRGB::RoyalBlue, CRGB::White};
      return &NeptunePalette_p;
    }
    case Planet::Sun: {
      static const TProgmemRGBPalette16 SunPalette_p FL_PROGMEM = {
          CRGB::Yellow, CRGB::Orange, CRGB::Red, CRGB::Gold, CRGB::Yellow, CRGB::Orange, CRGB::Red, CRGB::Gold,
          CRGB::Yellow, CRGB::Orange, CRGB::Red, CRGB::Gold, CRGB::Yellow, CRGB::Orange, CRGB::Red, CRGB::White};
      return &SunPalette_p;
    }
  }
  return &CloudColors_p;
}

}  // namespace

PlanetEffect* PlanetEffect::Get() {
  static PlanetEffect sPlanetEffect;
  return &sPlanetEffect;
}

PlanetEffect::PlanetEffect() {}

void PlanetEffect::SetPlanet(Planet planet) { currentPlanet_ = planet; }

size_t PlanetEffect::contextSize(const Frame& /*frame*/) const { return 0; }

void PlanetEffect::begin(const Frame& /*frame*/) const {}

void PlanetEffect::rewind(const Frame& /*frame*/) const {}

void PlanetEffect::afterColors(const Frame& /*frame*/) const {}

CRGB PlanetEffect::color(const Frame& frame, const Pixel& px) const {
  const TProgmemRGBPalette16* palette = GetPlanetPalette(currentPlanet_);
  uint8_t colorIndex = (px.cumulativeIndex * 16) + (256 * frame.time / kEffectDuration);
  return ColorFromPalette(*palette, colorIndex);
}

std::string PlanetEffect::effectName(PatternBits /*pattern*/) const {
  return std::string("planet-") + GetPlanetName(currentPlanet_);
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)
