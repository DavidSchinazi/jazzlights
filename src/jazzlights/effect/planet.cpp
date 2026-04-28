#include "jazzlights/effect/planet.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include "jazzlights/util/log.h"

namespace jazzlights {

namespace {

const TProgmemRGBPalette16* GetPlanetPalette(Planet planet) {
  switch (planet) {
    case Planet::Mercury: {
      static const TProgmemRGBPalette16 MercuryPalette_p FL_PROGMEM = {
          CRGB::Gray, CRGB::White, CRGB::DarkGray, CRGB::Gray, CRGB::White, CRGB::DarkGray,
          CRGB::Gray, CRGB::White, CRGB::DarkGray, CRGB::Gray, CRGB::White, CRGB::DarkGray,
          CRGB::Gray, CRGB::White, CRGB::DarkGray, CRGB::Gray};
      return &MercuryPalette_p;
    }
    case Planet::Venus: {
      static const TProgmemRGBPalette16 VenusPalette_p FL_PROGMEM = {
          CRGB::Yellow, CRGB::Gold, CRGB::LightYellow, CRGB::Yellow, CRGB::Gold, CRGB::LightYellow,
          CRGB::Yellow, CRGB::Gold, CRGB::LightYellow, CRGB::Yellow, CRGB::Gold, CRGB::LightYellow,
          CRGB::Yellow, CRGB::Gold, CRGB::LightYellow, CRGB::Yellow};
      return &VenusPalette_p;
    }
    case Planet::Earth: {
      static const TProgmemRGBPalette16 EarthPalette_p FL_PROGMEM = {
          CRGB::Blue,       CRGB::Green,    CRGB::DodgerBlue, CRGB::DarkGreen, CRGB::Blue,       CRGB::Green,
          CRGB::DodgerBlue, CRGB::DarkBlue, CRGB::Blue,       CRGB::Green,     CRGB::DodgerBlue, CRGB::DarkGreen,
          CRGB::Blue,       CRGB::Green,    CRGB::DodgerBlue, CRGB::DarkBlue};
      return &EarthPalette_p;
    }
    case Planet::Mars: {
      static const TProgmemRGBPalette16 MarsPalette_p FL_PROGMEM = {
          CRGB::OrangeRed, CRGB::DarkOrange, CRGB::Orange, CRGB::Maroon, CRGB::OrangeRed, CRGB::DarkOrange,
          CRGB::OrangeRed, CRGB::DarkOrange, CRGB::Orange, CRGB::Maroon, CRGB::OrangeRed, CRGB::DarkOrange,
          CRGB::OrangeRed, CRGB::DarkOrange, CRGB::Orange, CRGB::Maroon};
      return &MarsPalette_p;
    }
    case Planet::Jupiter: {
      static const TProgmemRGBPalette16 JupiterPalette_p FL_PROGMEM = {
          CRGB::Sienna, CRGB::Orange, CRGB::Chocolate, CRGB::Sienna, CRGB::Orange, CRGB::Chocolate,
          CRGB::Sienna, CRGB::Orange, CRGB::Chocolate, CRGB::Sienna, CRGB::Orange, CRGB::Chocolate,
          CRGB::Sienna, CRGB::Orange, CRGB::Chocolate, CRGB::Sienna};
      return &JupiterPalette_p;
    }
    case Planet::Saturn: {
      static const TProgmemRGBPalette16 SaturnPalette_p FL_PROGMEM = {
          CRGB::Wheat, CRGB::NavajoWhite, CRGB::Gold, CRGB::Wheat, CRGB::NavajoWhite, CRGB::Gold,
          CRGB::Wheat, CRGB::NavajoWhite, CRGB::Gold, CRGB::Wheat, CRGB::NavajoWhite, CRGB::Gold,
          CRGB::Wheat, CRGB::NavajoWhite, CRGB::Gold, CRGB::Wheat};
      return &SaturnPalette_p;
    }
    case Planet::Uranus: {
      static const TProgmemRGBPalette16 UranusPalette_p FL_PROGMEM = {
          CRGB::Cyan, CRGB::PaleTurquoise, CRGB::Aquamarine, CRGB::Cyan,
          CRGB::Cyan, CRGB::PaleTurquoise, CRGB::Aquamarine, CRGB::Cyan,
          CRGB::Cyan, CRGB::PaleTurquoise, CRGB::Aquamarine, CRGB::Cyan,
          CRGB::Cyan, CRGB::PaleTurquoise, CRGB::Aquamarine, CRGB::Cyan};
      return &UranusPalette_p;
    }
    case Planet::Neptune: {
      static const TProgmemRGBPalette16 NeptunePalette_p FL_PROGMEM = {
          CRGB::DarkBlue,     CRGB::Blue,     CRGB::MidnightBlue, CRGB::DarkBlue, CRGB::DarkBlue,     CRGB::Blue,
          CRGB::MidnightBlue, CRGB::DarkBlue, CRGB::DarkBlue,     CRGB::Blue,     CRGB::MidnightBlue, CRGB::DarkBlue,
          CRGB::DarkBlue,     CRGB::Blue,     CRGB::MidnightBlue, CRGB::DarkBlue};
      return &NeptunePalette_p;
    }
    case Planet::Sun: {
      static const TProgmemRGBPalette16 SunPalette_p FL_PROGMEM = {
          CRGB::Yellow, CRGB::Orange, CRGB::Red, CRGB::Gold, CRGB::Yellow, CRGB::Orange, CRGB::Red, CRGB::Gold,
          CRGB::Yellow, CRGB::Orange, CRGB::Red, CRGB::Gold, CRGB::Yellow, CRGB::Orange, CRGB::Red, CRGB::Orange};
      return &SunPalette_p;
    }
  }
  return &CloudColors_p;
}

uint8_t GetNumPixels(Planet planet) {
  switch (planet) {
    case Planet::Mercury: return 12;
    case Planet::Venus:
    case Planet::Mars: return 16;
    case Planet::Earth: return 24;
    case Planet::Jupiter:
    case Planet::Saturn:
    case Planet::Uranus:
    case Planet::Neptune:
    case Planet::Sun: return 35;
  }
  return 35;
}

}  // namespace

PlanetEffect* PlanetEffect::Get() {
  static PlanetEffect sPlanetEffect;
  return &sPlanetEffect;
}

PlanetEffect::PlanetEffect() { SetPlanet(Planet::Mercury); }

void PlanetEffect::SetPlanet(Planet planet) {
  currentPlanet_ = planet;
  numPixels_ = GetNumPixels(planet);
}

void PlanetEffect::SetHallSensorClosed(bool isClosed) { hallSensorClosed_ = isClosed; }

size_t PlanetEffect::contextSize(const Frame& /*frame*/) const { return sizeof(State); }

void PlanetEffect::begin(const Frame& /*frame*/) const {}

void PlanetEffect::rewind(const Frame& frame) const {
  State* state = static_cast<State*>(frame.context);
  state->half = (frame.pattern & kPlanetPatternHalfBit) != 0;
  state->hall = (frame.pattern & kPlanetPatternHallSensorBit) != 0;
  state->offset = (frame.pattern >> kPlanetPatternOffsetShift) & kPlanetPatternOffsetMask;
  state->hallSensorClosed = hallSensorClosed_;
}

void PlanetEffect::afterColors(const Frame& /*frame*/) const {}

CRGB PlanetEffect::color(const Frame& frame, const Pixel& px) const {
  if (px.cumulativeIndex >= numPixels_) { return CRGB::Black; }
#if !JL_ORRERY_SUN && !JL_ORRERY_PLUTO
  State* state = static_cast<State*>(frame.context);
  if (state->hall) { return state->hallSensorClosed ? CRGB::Blue : CRGB::Red; }
  if (state->half) {
    int offsetDistance = static_cast<int>(px.cumulativeIndex) - static_cast<int>(state->offset);
    if (offsetDistance < 0) { offsetDistance += numPixels_; }
    offsetDistance = std::min(offsetDistance, static_cast<int>(numPixels_) - offsetDistance);
    if (offsetDistance > numPixels_ / 4) { return CRGB::Black; }
  }
#endif  // !JL_ORRERY_SUN && !JL_ORRERY_PLUTO
  const TProgmemRGBPalette16* palette = GetPlanetPalette(currentPlanet_);
  uint8_t colorIndex = (256 * px.cumulativeIndex / numPixels_) + (256 * frame.time / kEffectDuration);
  return ColorFromPalette(*palette, colorIndex);
}

std::string PlanetEffect::effectName(PatternBits /*pattern*/) const {
  return std::string("planet-") + GetPlanetName(currentPlanet_);
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)
