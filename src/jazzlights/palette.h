#ifndef JAZZLIGHTS_PALETTE_H
#define JAZZLIGHTS_PALETTE_H

#include <cstdint>

#include "jazzlights/effect.h"
#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/util/color.h"

namespace jazzlights {

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

static inline const TProgmemRGBPalette16* FastLEDPaletteFromOurColorPalette(OurColorPalette ocp) {
  switch (ocp) {
    case OCPcloud: return &CloudColors_p;
    case OCPlava: return &JLLavaColors_p;
    case OCPocean: return &OceanColors_p;
    case OCPforest: return &ForestColors_p;
    case OCPrainbow: return &RainbowColors_p;
    case OCPparty: return &PartyColors_p;
    case OCPheat: return &HeatColors_p;
  }
  return FastLEDPaletteFromOurColorPalette(OCPrainbow);
}

static inline RgbColor colorFromOurPalette(OurColorPalette ocp, uint8_t color) {
  return (*FastLEDPaletteFromOurColorPalette(ocp))[color >> 4];
}

class ColorWithPalette {
 public:
  ColorWithPalette(uint8_t innerColor) : colorOverridden_(false), innerColor_(innerColor) {}
  static ColorWithPalette OverrideColor(RgbColor overrideColor) {
    ColorWithPalette col = ColorWithPalette();
    col.overrideColor_ = overrideColor;
    return col;
  }
  static ColorWithPalette OverrideCRGB(CRGB overrideColor) { return OverrideColor(overrideColor); }
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