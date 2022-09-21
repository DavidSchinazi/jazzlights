#ifndef JAZZLIGHTS_PALETTE_H
#define JAZZLIGHTS_PALETTE_H

#include <cstdint>

#include "jazzlights/effect.h"
#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/util/color.h"

namespace jazzlights {

// Some of the code here comes from FastLED
// https://github.com/FastLED/FastLED

typedef uint32_t TProgmemRGBPalette16[16];

extern const TProgmemRGBPalette16 cloudColors_p FL_PROGMEM = {
    CRGB::Blue,      CRGB::DarkBlue, CRGB::DarkBlue,  CRGB::DarkBlue,

    CRGB::DarkBlue,  CRGB::DarkBlue, CRGB::DarkBlue,  CRGB::DarkBlue,

    CRGB::Blue,      CRGB::DarkBlue, CRGB::SkyBlue,   CRGB::SkyBlue,

    CRGB::LightBlue, CRGB::White,    CRGB::LightBlue, CRGB::SkyBlue};

extern const TProgmemRGBPalette16 lavaColors_p FL_PROGMEM = {CRGB::Black,   CRGB::Maroon,  CRGB::Black,  CRGB::Maroon,

                                                             CRGB::DarkRed, CRGB::DarkRed, CRGB::Maroon, CRGB::DarkRed,

                                                             CRGB::DarkRed, CRGB::DarkRed, CRGB::Red,    CRGB::Orange,

                                                             CRGB::White,   CRGB::Orange,  CRGB::Red,    CRGB::DarkRed};

extern const TProgmemRGBPalette16 oceanColors_p FL_PROGMEM = {
    CRGB::MidnightBlue, CRGB::DarkBlue,   CRGB::MidnightBlue, CRGB::Navy,

    CRGB::DarkBlue,     CRGB::MediumBlue, CRGB::SeaGreen,     CRGB::Teal,

    CRGB::CadetBlue,    CRGB::Blue,       CRGB::DarkCyan,     CRGB::CornflowerBlue,

    CRGB::Aquamarine,   CRGB::SeaGreen,   CRGB::Aqua,         CRGB::LightSkyBlue};

extern const TProgmemRGBPalette16 forestColors_p FL_PROGMEM = {
    CRGB::DarkGreen,  CRGB::DarkGreen,        CRGB::DarkOliveGreen,   CRGB::DarkGreen,

    CRGB::Green,      CRGB::ForestGreen,      CRGB::OliveDrab,        CRGB::Green,

    CRGB::SeaGreen,   CRGB::MediumAquamarine, CRGB::LimeGreen,        CRGB::YellowGreen,

    CRGB::LightGreen, CRGB::LawnGreen,        CRGB::MediumAquamarine, CRGB::ForestGreen};

/// HSV Rainbow
extern const TProgmemRGBPalette16 rainbowColors_p FL_PROGMEM = {
    0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00, 0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
    0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5, 0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B};

/// HSV Rainbow colors with alternatating stripes of black
#define RainbowStripesColors_p RainbowStripeColors_p
extern const TProgmemRGBPalette16 rainbowStripeColors_p FL_PROGMEM = {
    0xFF0000, 0x000000, 0xAB5500, 0x000000, 0xABAB00, 0x000000, 0x00FF00, 0x000000,
    0x00AB55, 0x000000, 0x0000FF, 0x000000, 0x5500AB, 0x000000, 0xAB0055, 0x000000};

/// HSV color ramp: blue purple ping red orange yellow (and back)
/// Basically, everything but the greens, which tend to make
/// people's skin look unhealthy.  This palette is good for
/// lighting at a club or party, where it'll be shining on people.
extern const TProgmemRGBPalette16 partyColors_p FL_PROGMEM = {
    0x5500AB, 0x84007C, 0xB5004B, 0xE5001B, 0xE81700, 0xB84700, 0xAB7700, 0xABAB00,
    0xAB5500, 0xDD2200, 0xF2000E, 0xC2003E, 0x8F0071, 0x5F00A1, 0x2F00D0, 0x0007F9};

/// Approximate "black body radiation" palette, akin to
/// the FastLED 'HeatColor' function.
/// Recommend that you use values 0-240 rather than
/// the usual 0-255, as the last 15 colors will be
/// 'wrapping around' from the hot end to the cold end,
/// which looks wrong.
extern const TProgmemRGBPalette16 heatColors_p FL_PROGMEM = {0x000000, 0x330000, 0x660000, 0x990000, 0xCC0000, 0xFF0000,
                                                             0xFF3300, 0xFF6600, 0xFF9900, 0xFFCC00, 0xFFFF00, 0xFFFF33,
                                                             0xFFFF66, 0xFFFF99, 0xFFFFCC, 0xFFFFFF};

static inline RgbColor flToDf(CRGB ori) { return RgbColor(ori.r, ori.g, ori.b); }

#define ALL_COLORS \
  X(cloud)         \
  X(lava)          \
  X(ocean)         \
  X(forest)        \
  X(rainbow)       \
  X(party)         \
  X(heat)

enum OurColorPalette {
#define X(c) OCP##c,
  ALL_COLORS
#undef X
};

static inline OurColorPalette PaletteFromPattern(PatternBits pattern) {
  constexpr uint8_t firstPaletteBit = 17;
#if 0
  jll_info("PATTERN BITS %u%u%u",
       patternbit(pattern, firstPaletteBit + 0),
       patternbit(pattern, firstPaletteBit + 1),
       patternbit(pattern, firstPaletteBit + 2));
#endif
  if (patternbit(pattern, firstPaletteBit)) {        // nature
    if (patternbit(pattern, firstPaletteBit + 1)) {  // rainbow
      return OCPrainbow;
    } else {                                           // frolick
      if (patternbit(pattern, firstPaletteBit + 2)) {  // forest
        return OCPforest;
      } else {  // party
        return OCPparty;
      }
    }
  } else {                                             // hot&cold
    if (patternbit(pattern, firstPaletteBit + 1)) {    // cold
      if (patternbit(pattern, firstPaletteBit + 2)) {  // cloud
        return OCPcloud;
      } else {  // ocean
        return OCPocean;
      }
    } else {                                           // hot
      if (patternbit(pattern, firstPaletteBit + 2)) {  // lava
        return OCPlava;
      } else {  // heat
        return OCPheat;
      }
    }
  }
}

static inline RgbColor colorFromOurPalette(OurColorPalette ocp, uint8_t color) {
#define RET_COLOR(c) return flToDf(c##Colors_p[color >> 4])
#define X(c) \
  case OCP##c: RET_COLOR(c);
  switch (ocp) { ALL_COLORS }
  RET_COLOR(rainbow);
#undef X
#undef RET_COLOR
}

class ColorWithPalette {
 public:
  ColorWithPalette(uint8_t innerColor) : colorOverridden_(false), innerColor_(innerColor) {}
  static ColorWithPalette OverrideColor(RgbColor overrideColor) {
    ColorWithPalette col = ColorWithPalette();
    col.overrideColor_ = overrideColor;
    return col;
  }
  static ColorWithPalette OverrideCRGB(CRGB overrideColor) { return OverrideColor(flToDf(overrideColor)); }
  RgbColor colorFromPalette(OurColorPalette ocp) const {
    if (colorOverridden_) { return overrideColor_; }
    return colorFromOurPalette(ocp, innerColor_);
  }

 private:
  explicit ColorWithPalette() : colorOverridden_(true) {}
  bool colorOverridden_;
  uint8_t innerColor_;
  RgbColor overrideColor_;
};

inline std::string PaletteNameFromPattern(PatternBits pattern) {
  switch (PaletteFromPattern(pattern)) {
#define X(c) \
  case OCP##c: return #c;
    ALL_COLORS
#undef X
  }
  return "unknown";
}

template <typename STATE>
class EffectWithPaletteAndState : public Effect {
 protected:
 public:
  virtual std::string effectNamePrefix(PatternBits pattern) const = 0;
  virtual size_t extraContextSize(const Frame& frame) const {
    (void)frame;
    return 0;
  }
  virtual ColorWithPalette innerColor(const Frame& frame, const Pixel& px, STATE* state) const = 0;
  virtual void innerBegin(const Frame& frame, STATE* state) const {
    (void)frame;
    (void)state;
  }
  virtual void innerRewind(const Frame& frame, STATE* state) const {
    (void)frame;
    (void)state;
  }

  std::string effectName(PatternBits pattern) const override {
    return effectNamePrefix(pattern) + "-" + PaletteNameFromPattern(pattern);
  }

  Color color(const Frame& frame, const Pixel& px) const override {
    EffectWithPaletteState* state = reinterpret_cast<EffectWithPaletteState*>(frame.context);
    const ColorWithPalette colorWithPalette = innerColor(frame, px, &state->innerState);
    return colorWithPalette.colorFromPalette(state->ocp);
  }

  size_t contextSize(const Frame& frame) const override {
    return sizeof(EffectWithPaletteState) + extraContextSize(frame);
  }

  void begin(const Frame& frame) const override {
    EffectWithPaletteState* state = reinterpret_cast<EffectWithPaletteState*>(frame.context);
    state->ocp = PaletteFromPattern(frame.pattern);
    innerBegin(frame, &state->innerState);
  }

  void rewind(const Frame& frame) const override {
    EffectWithPaletteState* state = reinterpret_cast<EffectWithPaletteState*>(frame.context);
    innerRewind(frame, &state->innerState);
  }

 private:
  struct EffectWithPaletteState {
    OurColorPalette ocp;
    STATE innerState;
  };
};

template <typename STATE>
struct EffectWithPaletteState {
  OurColorPalette ocp;
  STATE innerState;
};

template <typename STATE, typename PER_PIXEL_TYPE>
class EffectWithPaletteXYIndexAndState : public XYIndexStateEffect<EffectWithPaletteState<STATE>, PER_PIXEL_TYPE> {
 protected:
  RgbColor colorFromPalette(uint8_t innerColor) const { return colorFromOurPalette(ocp_, innerColor); }

 public:
  virtual std::string effectNamePrefix(PatternBits pattern) const = 0;
  virtual ColorWithPalette innerColor(const Frame& frame, STATE* state, const Pixel& px) const = 0;
  virtual void innerBegin(const Frame& frame, STATE* state) const = 0;
  virtual void innerRewind(const Frame& frame, STATE* state) const = 0;

  std::string effectName(PatternBits pattern) const override {
    return effectNamePrefix(pattern) + "-" + PaletteNameFromPattern(pattern);
  }

  Color innerColor(const Frame& frame, EffectWithPaletteState<STATE>* state, const Pixel& px) const override {
    const ColorWithPalette colorWithPalette = innerColor(frame, &state->innerState, px);
    return colorWithPalette.colorFromPalette(state->ocp);
  }

  void innerBegin(const Frame& frame, EffectWithPaletteState<STATE>* state) const override {
    state->ocp = PaletteFromPattern(frame.pattern);
    ocp_ = state->ocp;
    innerBegin(frame, &state->innerState);
  }

  void innerRewind(const Frame& frame, EffectWithPaletteState<STATE>* state) const override {
    ocp_ = state->ocp;
    innerRewind(frame, &state->innerState);
  }

 private:
  mutable OurColorPalette ocp_;
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_PALETTE_H